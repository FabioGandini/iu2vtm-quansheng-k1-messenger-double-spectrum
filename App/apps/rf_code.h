#ifndef APPS_RF_CODE_H
#define APPS_RF_CODE_H

/* Shared layout for the RF Learn / RF Send companion pair (own gate/garage
 * remotes only). v3 -- DECODE AND REGENERATE.
 *
 * Earlier versions stored the literal captured envelope run-by-run and
 * replayed it, which carried the capture's 50us quantisation noise and
 * every per-edge error straight onto the air. Arduino+RXB6 ground truth
 * of this exact remote proved the signal is in fact a textbook PWM OOK
 * code: a constant bit period, each bit a short or long carrier pulse
 * (short=0, long=1), the rest of the period carrier-off, then a long
 * inter-frame gap, repeated while the button is held. For this remote:
 * short ~372us, long ~874us, period ~1048us, gap ~27ms, 25 bits
 * (0000000001000010010111101).
 *
 * So RF Learn now DECODES the capture into bits + the canonical short/
 * long/period/gap timings, and RF Send REGENERATES a mathematically
 * exact, perfectly periodic signal from them. The short-vs-long decision
 * has a ~500us margin (372 vs 874), so coarse-sampling capture noise
 * cannot flip a bit; and the replayed waveform is clean and identical in
 * character to the original (no more "different tone").
 *
 * Bump RF_CODE_MAGIC on any incompatible layout change. */
#define RF_CODE_MAGIC    0x33444652u   /* "RFD3" */
#define RF_CODE_MAX_BITS 64u            /* bits packed 8/byte -> 8 bytes  */
#define RF_CODE_LEN      22u            /* see offsets below              */

#define RF_LEARN_APP_NAME "RF Learn"
#define RF_SEND_APP_NAME  "RF Send"

/* Byte layout (little-endian):
 *   [0..3]   magic (RF_CODE_MAGIC)
 *   [4]      nBits (1..RF_CODE_MAX_BITS)
 *   [5]      flags (reserved, 0)
 *   [6..7]   shortUs  (u16) short pulse width, us
 *   [8..9]   longUs   (u16) long pulse width, us
 *   [10..11] periodUs (u16) one bit's full period (pulse + its gap), us
 *   [12..13] gapUs    (u16) inter-frame gap, us
 *   [14..21] bits, MSB-first within each byte, bit 0 = byte14 bit7
 */
#define RF_CODE_OFF_MAGIC   0u
#define RF_CODE_OFF_NBITS   4u
#define RF_CODE_OFF_FLAGS   5u
#define RF_CODE_OFF_SHORT   6u
#define RF_CODE_OFF_LONG    8u
#define RF_CODE_OFF_PERIOD  10u
#define RF_CODE_OFF_GAP     12u
#define RF_CODE_OFF_BITS    14u

#endif
