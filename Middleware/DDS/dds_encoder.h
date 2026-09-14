/**
 ******************************************************************************
 * @file    dds_encoder.h
 * @brief   DDS_Command → SPI Frame Encoder (multi-register full frame)
 *
 * Converts a DDS_Command into a complete AD9959 multi-register SPI frame:
 *   CSR(2B) + CFTW(5B) + ACR(4B) + CPOW(3B) = 14 bytes per channel.
 *
 * One frame fully configures one channel (frequency + amplitude + phase)
 * in a single CS-low window.
 *
 * Single-wire 1-bit serial mode only (SPI1, 8-bit DataSize, SDIO0 = MOSI).
 * The 2-bit dual-wire encoder (Encoder_FormatCommand + interleave helpers)
 * was removed 2026-08-08: it assumed 4-bit DataSize hardware that does not
 * exist in the current CubeMX configuration.
 ******************************************************************************
 */

#ifndef __DDS_ENCODER_H__
#define __DDS_ENCODER_H__

#include "dds_command.h"

/** Total bytes per channel in a multi-register full frame. */
#define ENCODER_FRAME_BYTES  14U

/** Flat (zero-copy DMA) encoding bytes per channel: csr[2] + cftw[5] + acr[4] + cpow[3]. */
#define ENCODER_FRAME_FLAT_BYTES  14U

/**
 * @brief  Pre-encoded data-register byte arrays for TIM8 ISR replay.
 *
 * CSR is included so that every runtime register transaction explicitly
 * addresses its intended channel.  CFR remains static and is written once
 * on init / mode change via Encoder_WriteStaticRegs().
 */
typedef struct {
    uint8_t csr[2];      /* [inst=0x00, channel-select mask]          */
    uint8_t cftw[5];     /* [inst=0x04, FTW[31:0] MSB-first]         */
    uint8_t acr[4];      /* [inst=0x06, ramp_rate=0, AMP_MULT, ASF]  */
    uint8_t cpow[3];     /* [inst=0x05, POW[15:0] MSB-first]         */
} DDS_EncodedFrame;
/**
 * @brief  Write static registers (CSR + CFR) for enabled channels.
 *
 * Called once after AD9959_Init() and whenever the active channel set
 * or modulation mode changes.  CSR takes effect immediately;
 * CFR configures sine-enable + DAC full-scale current.
 *
 * @param  hspi         : SPI1 handle (8-bit, blocking)
 * @param  channel_mask : bitmask of channels to configure (e.g. 1<<1)
 */
void Encoder_WriteStaticRegs(uint8_t channel_mask);

/**
 * @brief  Pre-encode data registers (CFTW+ACR+CPOW) for TIM8 ISR replay.
 *
 * Single-wire 1-bit mode only — no bit interleave.
 *
 * @param  cmd   : DDS_Command with all fields populated
 * @param  frame : [out] pre-encoded byte arrays
 */
void Encoder_Encode1Bit(const DDS_Command *cmd, DDS_EncodedFrame *frame);

/**
 * @brief  Encode a DDS_Command directly into a flat DMA buffer (zero-copy).
 *
 * Register-write logic is identical to Encoder_Encode1Bit(), but the bytes
 * are written directly into the caller-provided flat_buf[0..13] without an
 * intermediate DDS_EncodedFrame.  Used by DMA mode: the CPU encodes straight
 * into the D2-SRAM ping-pong buffer, then DMA sends it — no memcpy.
 *
 * @param  cmd      : DDS_Command with all fields populated
 * @param  flat_buf : destination buffer (>= ENCODER_FRAME_FLAT_BYTES bytes,
 *                    must reside in DMA-accessible SRAM)
 */
void Encoder_Encode1Bit_Direct(const DDS_Command *cmd, uint8_t *flat_buf);

#endif /* __DDS_ENCODER_H__ */
