/* RF Send (v3) -- REGENERATES a clean 433 MHz OOK signal from the bits +
 * timings RF Learn decoded, by keying the carrier on/off. FOR YOUR OWN
 * fixed-code gate/garage remote only.
 *
 * No noisy envelope replay: each bit is a mathematically exact pulse
 * (short=0, long=1) followed by carrier-off to fill the bit period, then
 * a long inter-frame gap, repeated while MENU is held -- identical in
 * character to the real remote. Loads RF Learn's saved block via the
 * resident config-proxy (gen_assets.py placeholder + asset_read).
 *
 * The CHANNEL must be in FM to transmit (AM TX is firmware-blocked, and
 * OOK carrier keying needs no AM anyway -- same as armel's CW Keyer).
 * Keys: MENU = transmit; EXIT = quit.
 */

#include <stdint.h>
#include <stdbool.h>
#include "../app_api.h"
#include "../rf_code.h"
_Static_assert(RF_CODE_LEN == 22u, "rf_code.h layout changed");

#define FIXED_FREQ_X10HZ 43392000u     /* 433.9200 MHz, VFO-independent   */

static const app_api_t *A;
static char str[40];

static uint8_t  codeBlock[RF_CODE_LEN];
static bool     haveCode;
static uint8_t  nBits;
static uint16_t shortUs, longUs, periodUs, gapUs;

static char *putu(char *o, uint32_t v)
{
    char t[10]; int8_t n = 0;
    do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v && n < 10);
    while (n--) *o++ = t[n];
    return o;
}
static char *put(char *o, const char *s) { while (*s) *o++ = *s++; return o; }
static void line(uint8_t row, const char *s) { A->print_normal(s, 0, 127, row); }

static uint16_t rd16(uint8_t off)
{
    return (uint16_t)(codeBlock[off] | (codeBlock[off + 1] << 8));
}

static void loadCode(void)
{
    A->asset_read(0, codeBlock, RF_CODE_LEN);
    uint32_t magic = (uint32_t)codeBlock[0] | ((uint32_t)codeBlock[1] << 8) |
                     ((uint32_t)codeBlock[2] << 16) | ((uint32_t)codeBlock[3] << 24);
    haveCode = (magic == RF_CODE_MAGIC) && codeBlock[RF_CODE_OFF_NBITS] > 0;
    if (haveCode) {
        nBits    = codeBlock[RF_CODE_OFF_NBITS];
        shortUs  = rd16(RF_CODE_OFF_SHORT);
        longUs   = rd16(RF_CODE_OFF_LONG);
        periodUs = rd16(RF_CODE_OFF_PERIOD);
        gapUs    = rd16(RF_CODE_OFF_GAP);
    }
}

static bool bitAt(uint8_t b)   /* MSB-first within each byte */
{
    return (codeBlock[RF_CODE_OFF_BITS + (b >> 3)] >> (7u - (b & 7u))) & 1u;
}

static void setLowPower(void)
{
    /* Real LOW1 calibration (radio.c): 3 bytes per band at 0x100D0 +
     * band*16 + bank*3 (bank 0 = LOW), scaled by the UHF LOW1 divider
     * (Txp*4/25), interpolated across band 6 (400-470 MHz) by frequency,
     * applied like BK4819_SetupPowerAmplifier (bias in REG_0x36 high byte,
     * PA-CTL enabled bit7, gain 0x22 for >=28 MHz). */
    uint8_t txp[3];
    A->sys_storage_read(0x100D0u + 6u * 16u, txp, 3);
    for (uint8_t p = 0; p < 3; p++) txp[p] = (uint8_t)(((uint16_t)txp[p] * 4u) / 25u);
    const int32_t lo = 40000000, up = 47000000, mid = 43500000, f = (int32_t)FIXED_FREQ_X10HZ;
    uint8_t bias = (f <= mid)
        ? (uint8_t)(txp[1] + ((int32_t)(txp[1] - txp[0]) * (f - lo)) / (mid - lo))
        : (uint8_t)(txp[1] + ((int32_t)(txp[2] - txp[1]) * (f - mid)) / (up - mid));
    A->bk_write(0x36, (uint16_t)(((uint16_t)bias << 8) | (1u << 7) | 0x22u));
}

static void transmitCode(void)
{
    if (A->tx_state() != 0u) { line(6, "TX denied: use FM"); A->blit_full(); return; }

    uint32_t (*const clk)(void) = A->cycles_48mhz;
    A->tx_set_params();
    A->bk_write(0x30, 0xC1FEu);                              /* TX enable flags */
    A->bk_write(0x38, (uint16_t)(FIXED_FREQ_X10HZ & 0xFFFFu));
    A->bk_write(0x39, (uint16_t)(FIXED_FREQ_X10HZ >> 16));
    setLowPower();
    A->delay_ms(20);                                         /* PLL/PA settle   */
    A->tx_carrier(false);

    /* Absolute-time scheduling absorbs each tx_carrier() SPI call's ~60us
     * cost (shortest interval here, long-pulse gap = period-long ~174us,
     * is well above it). Regenerated from clean decoded timings, not
     * captured runs. Repeat while MENU held (cap 20 ~ a second-plus). */
    uint32_t target = clk();
    for (uint8_t rep = 0; rep < 20 && A->get_key() == APP_KEY_MENU; rep++) {
        for (uint8_t b = 0; b < nBits; b++) {
            uint16_t pulse = bitAt(b) ? longUs : shortUs;
            uint16_t rest  = (periodUs > pulse) ? (uint16_t)(periodUs - pulse) : 0u;
            while ((int32_t)(clk() - target) < 0) { }
            A->tx_carrier(true);
            target += (uint32_t)pulse * 48u;
            while ((int32_t)(clk() - target) < 0) { }
            A->tx_carrier(false);
            target += (uint32_t)rest * 48u;
        }
        while ((int32_t)(clk() - target) < 0) { }
        target += (uint32_t)gapUs * 48u;                    /* inter-frame gap */
    }
    while ((int32_t)(clk() - target) < 0) { }
    A->tx_end();
}

static void show(void)
{
    char *o;
    A->display_clear();
    line(0, "RF SEND");
    if (!haveCode) {
        line(3, "No code saved");
        line(4, "Use RF Learn");
        line(5, "first (AM) to");
        line(6, "capture remote");
    } else {
        o = str; o = put(o, "bits:"); o = putu(o, nBits);
        o = put(o, " s:"); o = putu(o, shortUs); *o = 0; line(2, str);
        o = str; o = put(o, "l:"); o = putu(o, longUs);
        o = put(o, " p:"); o = putu(o, periodUs); *o = 0; line(3, str);
        o = str; o = put(o, "gap:"); o = putu(o, gapUs); *o = 0; line(4, str);
        line(5, "channel = FM !");
        line(6, "MENU=transmit");
    }
    A->blit_full();
}

__attribute__((section(".text.entry"),used))
void app_main(const app_api_t *api)
{
    A = api;
    A->backlight_on();
    loadCode();
    show();

    bool menuHeld = false;
    for (;;) {
        uint8_t key = A->get_key();
        if (key == APP_KEY_EXIT) break;
        bool menuNow = (key == APP_KEY_MENU);
        if (menuNow && !menuHeld && haveCode) {
            line(6, "Sending..."); A->blit_full();
            transmitCode();
            line(6, "Sent!"); A->blit_full();
            A->play_tone(1200, 150);
            A->delay_ms(300);
            show();
        }
        menuHeld = menuNow;
        A->delay_ms(20);
        A->backlight_update();
    }
}
