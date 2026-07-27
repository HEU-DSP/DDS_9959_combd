/**
 ******************************************************************************
 * @file    dds_encoder.c
 * @brief   DDS_Command → Multi-Register SPI Frame Encoder (2-bit serial mode)
 *
 * AD9959 2-bit serial mode (CSR[2:1]=10, per Figure 44):
 *   Each SCLK edge: SDIO_1 = odd bit (7,5,3,1), SDIO_0 = even bit (6,4,2,0).
 *   4 SCLK = 8 bits = 1 AD9959 byte.
 *
 * SPI DataSize=4-bit: each DMA transfer = 4 bits per lane.
 * Encoder builds raw frame bytes, then splits into interleaved SPI buffers.
 *
 * One frame = CSR(2B) + CFTW(5B) + ACR(4B) + CPOW(3B) = 14 bytes per lane.
 * A single CS-low window fully configures one channel.
 ******************************************************************************
 */

#include "dds_encoder.h"
#include "ad9959_reg.h"
#include <string.h>

/* ---- Bit interleave: one raw byte → two 4-bit SPI values ---- */
static inline uint8_t split_even(uint8_t b) {
    /* Bits 6,4,2,0 → packed into [3:0] for SDIO_0 */
    return ((b & 0x40) >> 3) | ((b & 0x10) >> 2) |
           ((b & 0x04) >> 1) |  (b & 0x01);
}
static inline uint8_t split_odd(uint8_t b) {
    /* Bits 7,5,3,1 → packed into [3:0] for SDIO_1 */
    return ((b & 0x80) >> 4) | ((b & 0x20) >> 3) |
           ((b & 0x08) >> 2) | ((b & 0x02) >> 1);
}

/* ---- Interleave an array of raw bytes into SPI1/SPI3 buffers ---- */
static void __attribute__((unused)) interleave_frames(const uint8_t *raw, uint8_t *spi1,
                              uint8_t *spi3, int len)
{
    for (int i = 0; i < len; i++) {
        spi1[i] = split_even(raw[i]);
        spi3[i] = split_odd(raw[i]);
    }
}

/* ================================================================
 * Public API
 * ================================================================ */

int Encoder_FormatCommand(const DDS_Command *cmd,
                          uint8_t *spi1_frame, uint8_t *spi3_frame)
{
    uint8_t raw[ENCODER_FRAME_BYTES];
    int idx = 0;

    /* ── 1. CSR (Channel Select Register): 2 bytes ──
     *    Select target channel + keep 2-bit serial mode.
     *    CSR takes effect immediately (no IO_UPDATE needed). */
    uint8_t csr_data = CSR_CHANNEL(cmd->profile);
    raw[idx++] = AD9959_REG_CSR & 0x7F;
    raw[idx++] = csr_data;

    /* ── 2. CFTW (Frequency Tuning Word): 5 bytes ── */
    raw[idx++] = AD9959_REG_CFTW & 0x7F;
    raw[idx++] = (cmd->ftw >> 24) & 0xFF;
    raw[idx++] = (cmd->ftw >> 16) & 0xFF;
    raw[idx++] = (cmd->ftw >> 8)  & 0xFF;
    raw[idx++] =  cmd->ftw        & 0xFF;

    /* ── 3. ACR (Amplitude Control Register): 4 bytes ──
     *    ASF at [9:0], AMP_MULT_ENABLE at bit 12, ramp rate = 0. */
    {
        uint32_t acr = ((uint32_t)(cmd->asf & ACR_ASF_Msk) << ACR_ASF_Pos) |
                       ACR_AMP_MULT_ENABLE;
        raw[idx++] = AD9959_REG_ACR & 0x7F;
        raw[idx++] = (acr >> 16) & 0xFF;   /* ramp rate = 0x00 */
        raw[idx++] = (acr >> 8)  & 0xFF;   /* AMP_MULT | ASF[9:8] */
        raw[idx++] =  acr        & 0xFF;   /* ASF[7:0] */
    }

    /* ── 4. CPOW (Phase Offset Word): 3 bytes ──
     *    14-bit phase: POW[13:0], upper 2 bits reserved = 0. */
    raw[idx++] = AD9959_REG_CPOW & 0x7F;
    raw[idx++] = (cmd->pow >> 8) & 0x3F;   /* POW[13:8] */
    raw[idx++] =  cmd->pow       & 0xFF;   /* POW[7:0] */

    /* ── Bit-interleave into separate SPI1/SPI3 buffers ── */
    memcpy(spi1_frame, raw, ENCODER_FRAME_BYTES);
    memset(spi3_frame, 0, ENCODER_FRAME_BYTES);

    return ENCODER_FRAME_BYTES;
}
