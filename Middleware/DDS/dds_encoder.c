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

/*
 * AD9959 Phase/Sweep Accumulator Clear Control Description:
 *
 * The phase accumulator clear function is controlled by multiple sources:
 *
 * 1. Channel level control:
 *    - CFR.Bit1: Clear Phase Accumulator
 *      When set to 1, the corresponding channel phase accumulator is held
 *      in the cleared state until this bit is released.
 *
 *    - CFR.Bit2: Autoclear Phase Accumulator
 *      When enabled, an I/O_UPDATE operation generates a clear request for
 *      the phase accumulator. The accumulator is cleared once and then
 *      resumes operation after the clear condition is removed.
 *
 * 2. Global control:
 *    - FR2.All Channels Clear Phase Accumulator
 *      Provides a global clear request for all channels.
 *
 *    - FR2.All Channels Autoclear Phase Accumulator
 *      Provides a global autoclear request for all channels on I/O_UPDATE.
 *
 * 3. Clear mode:
 *    - FR2 Phase/Sweep Accumulator Clear Mode
 *      Does not generate a clear request. It only determines how the clear
 *      request is applied:
 *      synchronous clear: accumulator is cleared on the system clock edge;
 *      asynchronous clear: accumulator is cleared immediately when the
 *      clear request becomes active.
 *
 * The final clear condition can be considered as:
 *
 *   Clear_Request =
 *       CFR.Bit1
 *     | CFR.Bit2 (triggered by I/O_UPDATE)
 *     | FR2.All_Clear
 *     | FR2.All_Autoclear (triggered by I/O_UPDATE)
 *
 * For continuous wave output and continuous phase modulation:
 *   CFR.Bit1  = 0
 *   CFR.Bit2  = 0
 *   FR2.All_Clear = 0
 *   FR2.All_Autoclear = 0
 *
 * Otherwise, every I/O_UPDATE may introduce phase discontinuity and
 * generate unwanted spectral spurs.
 */

#include "dds_encoder.h"
#include "ad9959_reg.h"
#include "ad9959.h"
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
static void interleave_frames(const uint8_t *raw, uint8_t *spi1,
                              uint8_t *spi3, int len)
{
    for (int i = 0; i < len; i++) {
        spi1[i] = split_even(raw[i]);
        spi3[i] = split_odd(raw[i]);
    }
}

/* ================================================================
 * Public API — 1-bit single-wire encoder
 * ================================================================ */

void Encoder_WriteStaticRegs(uint8_t channel_mask)
{
    /* CFR data is identical for every channel. */
    const uint8_t cfr_data[3] = { 0x00U, 0x03U, 0x00U };

    for (uint8_t ch = 0; ch < 4U; ch++) {
        if (!(channel_mask & (1U << ch))) continue;

        uint8_t csr_data = CSR_CHANNEL(ch);
        AD9959_WriteRegister(AD9959_REG_CSR, &csr_data, 1);
        AD9959_WriteRegister(AD9959_REG_CFR, cfr_data, 3);
    }
}

void Encoder_Encode1Bit(const DDS_Command *cmd, DDS_EncodedFrame *frame)
{
    
    /* CSR must precede every channel's data registers.  The AD9959 retains
     * the last CSR selection, so omitting this would send all later frames
     * to whichever channel happened to be configured last at startup. */
    frame->csr[0] = AD9959_REG_CSR & 0x7FU;
    frame->csr[1] = CSR_CHANNEL(cmd->profile);

    
    /* ── CFTW: 5 bytes ── */
    frame->cftw[0] = AD9959_REG_CFTW & 0x7F;
    frame->cftw[1] = (cmd->ftw >> 24) & 0xFF;
    frame->cftw[2] = (cmd->ftw >> 16) & 0xFF;
    frame->cftw[3] = (cmd->ftw >> 8)  & 0xFF;
    frame->cftw[4] =  cmd->ftw        & 0xFF;

    /* ── ACR: 4 bytes ── */
    {
        uint32_t acr = ((uint32_t)(cmd->asf & ACR_ASF_Msk) << ACR_ASF_Pos) |
                       ACR_AMP_MULT_ENABLE;
        frame->acr[0] = AD9959_REG_ACR & 0x7F;
        frame->acr[1] = (acr >> 16) & 0xFF;
        frame->acr[2] = (acr >> 8)  & 0xFF;
        frame->acr[3] =  acr        & 0xFF;
    }

    /* ── CPOW: 3 bytes ── */
    frame->cpow[0] = AD9959_REG_CPOW & 0x7F;
    frame->cpow[1] = (cmd->pow >> 8) & 0x3F;
    frame->cpow[2] =  cmd->pow       & 0xFF;
}

/* ================================================================
 * Public API — 2-bit dual-wire encoder (original)
 * ================================================================ */

int Encoder_FormatCommand(const DDS_Command *cmd,
                          uint8_t *spi1_frame, uint8_t *spi3_frame)
{
    uint8_t raw[ENCODER_FRAME_BYTES];
    int idx = 0;

    /* ── 1. CSR (Channel Select Register): 2 bytes ──
     *    Select target channel + keep 2-bit serial mode.
     *    CSR takes effect immediately (no IO_UPDATE needed). */
    uint8_t csr_data = CSR_CHANNEL(cmd->profile) | CSR_IO_MODE_2BIT;
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
    interleave_frames(raw, spi1_frame, spi3_frame, ENCODER_FRAME_BYTES);

    return ENCODER_FRAME_BYTES;
}
