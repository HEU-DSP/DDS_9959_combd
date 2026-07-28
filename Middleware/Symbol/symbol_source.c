/**
 ******************************************************************************
 * @file    symbol_source.c
 * @brief   Local auto symbol generator implementation.
 ******************************************************************************
 */

#include "symbol_source.h"
#include "symbol_buffer.h"

/* ---- 32-point sine table for AM/FM test (Q15, -32767..32767) ---- */
static const int16_t sine32[32] = {
        0,   6393,  12540,  18205,  23170,  27246,  30274,  32138,
    32767,  32138,  30274,  27246,  23170,  18205,  12540,   6393,
        0,  -6393, -12540, -18205, -23170, -27246, -30274, -32138,
   -32767, -32138, -30274, -27246, -23170, -18205, -12540,  -6393,
};
static uint8_t sine_idx = 0U;

/* ---- Toggle state for FSK/ASK/BPSK/GFSK/MSK ---- */
static uint8_t toggle_bit = 0U;
static uint8_t qpsk_phase  = 0U;   /* cycles 0,1,2,3 */

/* ================================================================
 * Public API
 * ================================================================ */

void SymbolSource_Generate(uint8_t input_bits)
{
    if (!SymbolBuf_IsFree()) {
        return;
    }
    if (input_bits == 0U) {
        return;   /* CW mode — no symbols needed */
    }

    toggle_bit ^= 1U;

    switch (input_bits) {
    case 1U:
        /* FSK / ASK / BPSK / GFSK / MSK: alternating 0,1 */
        SymbolBuf_WriteBits(&toggle_bit, 1U);
        break;

    case 2U:
        /* QPSK / 4FSK: cycle through Gray-coded symbols */
        {
            uint8_t sym[1];
            qpsk_phase = (qpsk_phase + 1U) & 0x03U;
            switch (qpsk_phase) {
            case 0U: sym[0] = 0x00U; break;  /* 00 */
            case 1U: sym[0] = 0x01U; break;  /* 01 */
            case 2U: sym[0] = 0x03U; break;  /* 11 */
            case 3U: sym[0] = 0x02U; break;  /* 10 */
            }
            SymbolBuf_WriteBits(sym, 2U);
        }
        break;

    default:
        /* Unknown — fall back to alternating bit */
        SymbolBuf_WriteBits(&toggle_bit, 1U);
        break;
    }

    (void)sine32;
    (void)sine_idx;
}
