/* RF Learn (v3) -- captures a 433 MHz OOK fixed-code remote (your own
 * gate/garage remote) and DECODES it into bits + canonical timings for
 * RF Send to regenerate cleanly. Companion to RF Send via the resident
 * config-proxy mechanism (rf_code.h, app_config_proxy.h): this app calls
 * cfg_save() normally, the loader redirects it into RF Send's asset area.
 *
 * Pipeline: af_capture on PA4 at 20 kHz -> hi/lo envelope with a dynamic
 * squelch-hold slicer and a 2-sample debounce -> run extraction -> DECODE
 * (classify each carrier pulse short/long against a mid threshold with a
 * ~500us margin, measure the canonical short/long/period/gap, pack bits).
 *
 * Channel must be in AM to receive. Tune the VFO to the remote's freq
 * (433.9200 MHz); this app also sets 433.9200 AM/wide + raw-audio regs
 * itself. Keys: MENU = learn; EXIT = quit.
 */

#include <stdint.h>
#include <stdbool.h>
#include "../app_api.h"
#include "../rf_code.h"
_Static_assert(RF_CODE_LEN == 22u, "rf_code.h layout changed");

#define BUF_BYTES    768u
#define N_SAMPLES    (BUF_BYTES * 8u)
#define CYC_20K      2400u             /* 48 MHz / 20 kHz = 50 us/sample  */
#define MAX_RUNS     (RF_CODE_MAX_BITS * 2u)

#define FIXED_FREQ_X10HZ 43392000u     /* 433.9200 MHz, VFO-independent   */

static const app_api_t *A;

static uint8_t  buf[BUF_BYTES];
static uint16_t runUs[MAX_RUNS];       /* each run's duration in us       */
static int32_t  dcCenter, noiseAmp;
static uint32_t realSampleUs;
static char     str[40];
static bool     haveResult, saved;

static uint8_t  codeBlock[RF_CODE_LEN];
/* decode results (also used for the display) */
static uint16_t dShort, dLong, dPeriod, dGap;
static uint8_t  dNBits;
static bool     dClean;

static char *putu(char *o, uint32_t v)
{
    char t[10]; int8_t n = 0;
    do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v && n < 10);
    while (n--) *o++ = t[n];
    return o;
}
static char *put(char *o, const char *s) { while (*s) *o++ = *s++; return o; }
static void line(uint8_t row, const char *s) { A->print_normal(s, 0, 127, row); }

static void configureRawAudio(void)
{
    A->bk_write(0x38, (uint16_t)(FIXED_FREQ_X10HZ & 0xFFFFu));
    A->bk_write(0x39, (uint16_t)(FIXED_FREQ_X10HZ >> 16));
    A->bk_write(0x43, 0x7E28u);                              /* widest RX filter */
    uint16_t r2b = A->bk_read(0x2B);
    A->bk_write(0x2B, (uint16_t)((r2b | 0x0700u) & ~0x0007u)); /* RX AF filters off */
    A->bk_write(0x73, (uint16_t)(A->bk_read(0x73) | 0x0010u)); /* AFC off */
    A->set_agc(false);
    A->bk_write(0x30, (uint16_t)(A->bk_read(0x30) | (1u << 9))); /* AF DAC -> PA4 */
    A->audio_path(false);
    A->set_af(APP_AF_AM);
}

static void calibrate(void)
{
    uint32_t (*const clk)(void) = A->cycles_48mhz;
    uint32_t next = clk();
    for (uint16_t i = 0; i < 500; i++) {          /* settle the AC-coupled bias */
        while ((int32_t)(clk() - next) < 0) { }
        next += CYC_20K;
        (void)A->af_capture_sample();
    }
    int32_t sum = 0, sumAbs = 0;
    const uint16_t n = 200;
    next = clk();
    for (uint16_t i = 0; i < n; i++) {
        while ((int32_t)(clk() - next) < 0) { }
        next += CYC_20K;
        sum += (int32_t)A->af_capture_sample();
    }
    dcCenter = sum / n;
    next = clk();
    for (uint16_t i = 0; i < n; i++) {
        while ((int32_t)(clk() - next) < 0) { }
        next += CYC_20K;
        int32_t d = (int32_t)A->af_capture_sample() - dcCenter;
        sumAbs += (d < 0) ? -d : d;
    }
    noiseAmp = sumAbs / n;
}

static void record(void)
{
    for (uint16_t i = 0; i < BUF_BYTES; i++) buf[i] = 0;

    uint32_t (*const clk)(void) = A->cycles_48mhz;
    int32_t x1v, x2v, x3v;
    x1v = x2v = x3v = (int32_t)A->af_capture_sample();
    int32_t hi = x1v, lo = x1v;
    bool state = false, cand = false; uint8_t debCnt = 0;

    /* pre-trigger: wait up to 2s for a real state-crossing above noise */
    {
        uint32_t until2 = A->ticks_ms() + 2000u;
        uint32_t w = clk();
        while (A->ticks_ms() < until2) {
            while ((int32_t)(clk() - w) < 0) { }
            w += CYC_20K;
            int32_t x0v = (int32_t)A->af_capture_sample();
            int32_t x = (x0v + x1v + x2v + x3v) >> 2;
            x3v = x2v; x2v = x1v; x1v = x0v;
            hi = (x > hi) ? x : hi - ((hi - x) >> 8);
            lo = (x < lo) ? x : lo + ((x - lo) >> 8);
            int32_t mid0 = (hi + lo) >> 1, amp0 = (hi - lo) >> 1;
            if (amp0 > noiseAmp * 3 && (x > mid0) != state) break;
        }
    }

    uint32_t tStart = clk(), w = clk();
    for (uint32_t s = 0; s < N_SAMPLES; s++) {
        while ((int32_t)(clk() - w) < 0) { }
        w += CYC_20K;
        int32_t x0v = (int32_t)A->af_capture_sample();
        int32_t x = (x0v + x1v + x2v + x3v) >> 2;
        x3v = x2v; x2v = x1v; x1v = x0v;
        hi = (x > hi) ? x : hi - ((hi - x) >> 8);
        lo = (x < lo) ? x : lo + ((x - lo) >> 8);
        int32_t mid = (hi + lo) >> 1, amp = (hi - lo) >> 1;
        bool raw = (amp > noiseAmp * 2) ? (x > mid) : state;   /* squelch-hold */
        if (raw != cand) { cand = raw; debCnt = 0; }
        else if (debCnt < 2) { debCnt++; if (debCnt == 2) state = cand; }
        if (state) buf[s >> 3] |= (uint8_t)(1u << (s & 7u));
    }
    uint32_t totalUs = (clk() - tStart) / 48u;
    realSampleUs = totalUs / N_SAMPLES;
    if (realSampleUs == 0) realSampleUs = 1;
}

static bool sampleAt(uint32_t i) { return (buf[i >> 3] >> (i & 7u)) & 1u; }

static void analyze(void)
{
    /* idle (quiet) level = majority of the first 16 samples */
    uint8_t ones = 0;
    for (uint32_t i = 0; i < 16; i++) ones += sampleAt(i) ? 1 : 0;
    bool idle = (ones > 8);

    /* Collect runs (us) from the first non-idle sample onward, alternating
     * pulse,gap,pulse,gap,...  Stop at the first gap >5000us -> that is the
     * inter-frame gap (one repeat captured). */
    uint8_t nRuns = 0;
    dGap = 27000;                       /* fallback if none seen           */
    bool started = false;
    uint32_t runStart = 0;
    bool cur = sampleAt(0);
    for (uint32_t i = 1; i <= N_SAMPLES && nRuns < MAX_RUNS; i++) {
        bool v = (i < N_SAMPLES) ? sampleAt(i) : !cur;
        if (v != cur) {
            uint32_t us = (i - runStart) * realSampleUs;
            if (!started) {
                if (cur != idle) {      /* first real run (a pulse) */
                    started = true;
                    runUs[nRuns++] = (us > 65535u) ? 65535u : (uint16_t)us;
                }
            } else {
                bool curIsGap = (cur == idle);
                if (curIsGap && us > 5000u) { dGap = (us > 65535u) ? 65535u : (uint16_t)us; break; }
                runUs[nRuns++] = (us > 65535u) ? 65535u : (uint16_t)us;
            }
            cur = v; runStart = i;
        }
    }

    /* pulses are the even-indexed runs (run 0 is the first pulse) */
    uint8_t nBits = (uint8_t)((nRuns + 1u) / 2u);
    if (nBits > RF_CODE_MAX_BITS) nBits = RF_CODE_MAX_BITS;

    uint16_t pmin = 0xFFFF, pmax = 0;
    for (uint8_t b = 0; b < nBits; b++) {
        uint16_t p = runUs[b * 2u];
        if (p < pmin) pmin = p;
        if (p > pmax) pmax = p;
    }
    uint16_t thr = (uint16_t)(((uint32_t)pmin + pmax) / 2u);

    uint32_t sShort = 0, sLong = 0, sPeriod = 0;
    uint8_t  nShort = 0, nLong = 0, nPeriod = 0;
    for (uint8_t b = 0; b < RF_CODE_MAX_BITS / 8u; b++) codeBlock[RF_CODE_OFF_BITS + b] = 0;
    for (uint8_t b = 0; b < nBits; b++) {
        uint16_t p = runUs[b * 2u];
        bool isLong = (p > thr);
        if (isLong) { sLong += p; nLong++;
                      codeBlock[RF_CODE_OFF_BITS + (b >> 3)] |= (uint8_t)(0x80u >> (b & 7u)); }
        else        { sShort += p; nShort++; }
        /* period = pulse + its following gap, for bits that have one */
        uint8_t gi = b * 2u + 1u;
        if (gi < nRuns) { sPeriod += (uint32_t)p + runUs[gi]; nPeriod++; }
    }
    dShort  = nShort ? (uint16_t)(sShort / nShort) : 0;
    dLong   = nLong  ? (uint16_t)(sLong  / nLong)  : 0;
    dPeriod = nPeriod ? (uint16_t)(sPeriod / nPeriod) : 0;
    dNBits  = nBits;

    /* Clean: enough bits, both symbol kinds present, a believable short/
     * long ratio (the decode margin is huge, so this is just a sanity gate). */
    dClean = false;
    if (nBits >= 8 && nShort >= 1 && nLong >= 1 && dShort > 0 && dPeriod > dLong) {
        uint32_t ratio = ((uint32_t)dLong * 100u) / dShort;
        dClean = (ratio >= 140u && ratio <= 400u);
    }

    codeBlock[RF_CODE_OFF_NBITS]  = nBits;
    codeBlock[RF_CODE_OFF_FLAGS]  = 0;
    codeBlock[RF_CODE_OFF_SHORT]  = (uint8_t)(dShort & 0xFFu);
    codeBlock[RF_CODE_OFF_SHORT+1]= (uint8_t)(dShort >> 8);
    codeBlock[RF_CODE_OFF_LONG]   = (uint8_t)(dLong & 0xFFu);
    codeBlock[RF_CODE_OFF_LONG+1] = (uint8_t)(dLong >> 8);
    codeBlock[RF_CODE_OFF_PERIOD] = (uint8_t)(dPeriod & 0xFFu);
    codeBlock[RF_CODE_OFF_PERIOD+1]=(uint8_t)(dPeriod >> 8);
    codeBlock[RF_CODE_OFF_GAP]    = (uint8_t)(dGap & 0xFFu);
    codeBlock[RF_CODE_OFF_GAP+1]  = (uint8_t)(dGap >> 8);
    codeBlock[RF_CODE_OFF_MAGIC]  = (uint8_t)(RF_CODE_MAGIC & 0xFFu);
    codeBlock[RF_CODE_OFF_MAGIC+1]= (uint8_t)((RF_CODE_MAGIC >> 8) & 0xFFu);
    codeBlock[RF_CODE_OFF_MAGIC+2]= (uint8_t)((RF_CODE_MAGIC >> 16) & 0xFFu);
    codeBlock[RF_CODE_OFF_MAGIC+3]= (uint8_t)((RF_CODE_MAGIC >> 24) & 0xFFu);
}

static void show(void)
{
    char *o;
    A->display_clear();
    line(0, "RF LEARN");
    if (!haveResult) {
        line(3, "MENU=learn");
        line(4, "433.9200 AM");
        line(5, "hold remote key");
        line(6, "while MENU held");
    } else {
        A->print_bold(dClean ? "CLEAN" : "noisy", 0, 127, 2);
        o = str; o = put(o, "bits:"); o = putu(o, dNBits);
        o = put(o, " s:"); o = putu(o, dShort); *o = 0; line(3, str);
        o = str; o = put(o, "l:"); o = putu(o, dLong);
        o = put(o, " p:"); o = putu(o, dPeriod); *o = 0; line(4, str);
        o = str; o = put(o, "gap:"); o = putu(o, dGap); *o = 0; line(5, str);
        line(6, saved ? "SAVED, MENU=again" : "noisy, MENU=again");
    }
    A->blit_full();
}

__attribute__((section(".text.entry"),used))
void app_main(const app_api_t *api)
{
    A = api;
    haveResult = false; saved = false;
    A->backlight_on();
    configureRawAudio();
    show();

    bool menuHeld = false;
    for (;;) {
        uint8_t key = A->get_key();
        if (key == APP_KEY_EXIT) break;
        bool menuNow = (key == APP_KEY_MENU);
        if (menuNow && !menuHeld) {
            line(6, "Waiting sig"); A->blit_full();
            A->af_capture_open();
            calibrate();
            record();
            A->af_capture_close();
            analyze();
            haveResult = true; saved = false;
            if (dClean) { A->cfg_save(codeBlock, RF_CODE_LEN); saved = true; }
            show();
        }
        menuHeld = menuNow;
        A->delay_ms(20);
        A->backlight_update();
    }
}
