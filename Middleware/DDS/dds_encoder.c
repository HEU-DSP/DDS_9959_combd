/**
 ******************************************************************************
 * @file    dds_encoder.c
 * @brief   DDS_Command → Multi-Register SPI Frame Encoder
 *
 * Active path is single-wire 1-bit serial mode: SPI1 only, 8-bit DataSize,
 * SDIO0 = SPI1_MOSI.  One frame = CSR(2B) + CFTW(5B) + ACR(4B) + CPOW(3B)
 * = 14 bytes per channel.  A single CS-low window may carry back-to-back
 * register writes (instruction bytes auto-detected at register boundaries).
 *
 * Encoder_Encode1Bit() packs a DDS_Command into a DDS_EncodedFrame for
 * the downgrade TIM8 ISR (per-field blocking SPI).
 * Encoder_Encode1Bit_Direct() packs the same 14 bytes flat into a DMA
 * buffer (DMA mode, zero-copy).
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

void Encoder_Encode1Bit_Direct(const DDS_Command *cmd, uint8_t *flat_buf)
{
    uint16_t off = 0;

    /* ── CSR: 2 bytes ── */
    flat_buf[off + 0] = AD9959_REG_CSR & 0x7FU;
    flat_buf[off + 1] = CSR_CHANNEL(cmd->profile);
    off += 2;

    /* ── CFTW: 5 bytes ── */
    flat_buf[off + 0] = AD9959_REG_CFTW & 0x7F;
    flat_buf[off + 1] = (cmd->ftw >> 24) & 0xFF;
    flat_buf[off + 2] = (cmd->ftw >> 16) & 0xFF;
    flat_buf[off + 3] = (cmd->ftw >> 8)  & 0xFF;
    flat_buf[off + 4] =  cmd->ftw        & 0xFF;
    off += 5;

    /* ── ACR: 4 bytes ── */
    {
        uint32_t acr = ((uint32_t)(cmd->asf & ACR_ASF_Msk) << ACR_ASF_Pos) |
                       ACR_AMP_MULT_ENABLE;
        flat_buf[off + 0] = AD9959_REG_ACR & 0x7F;
        flat_buf[off + 1] = (acr >> 16) & 0xFF;
        flat_buf[off + 2] = (acr >> 8)  & 0xFF;
        flat_buf[off + 3] =  acr        & 0xFF;
    }
    off += 4;

    /* ── CPOW: 3 bytes ── */
    flat_buf[off + 0] = AD9959_REG_CPOW & 0x7F;
    flat_buf[off + 1] = (cmd->pow >> 8) & 0x3F;
    flat_buf[off + 2] =  cmd->pow       & 0xFF;
    off += 3;

    /* off == 14 == ENCODER_FRAME_FLAT_BYTES */
}

/* Encoder_FormatCommand (2-bit dual-wire interleave) removed 2026-08-08:
 * it was only reachable through the deleted FrameBuilder/mod_* tree and
 * assumed 4-bit DataSize, which does not match the 8-bit SPI hardware. */
