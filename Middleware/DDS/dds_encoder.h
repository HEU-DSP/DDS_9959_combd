/**
 ******************************************************************************
 * @file    dds_encoder.h
 * @brief   DDS_Command → SPI Frame Encoder (multi-register full frame)
 *
 * Converts a DDS_Command into a complete AD9959 multi-register SPI frame:
 *   CSR(2B) + CFTW(5B) + ACR(4B) + CPOW(3B) = 14 bytes per lane.
 *
 * One frame fully configures one channel (frequency + amplitude + phase)
 * in a single CS-low window.
 *
 * For 2-bit serial mode: SDIO_1 = odd bits, SDIO_0 = even bits.
 * Each raw byte is split into two 4-bit SPI values via interleave.
 ******************************************************************************
 */

#ifndef __DDS_ENCODER_H__
#define __DDS_ENCODER_H__

#include "dds_command.h"
#include "stm32h7xx_hal.h"    /* for SPI_HandleTypeDef in Encoder_WriteStaticRegs */

/** Total bytes per SPI lane in a multi-register full frame. */
#define ENCODER_FRAME_BYTES  14U

/**
 * @brief  Pre-encoded data-register byte arrays for TIM8 ISR replay.
 *
 * Only the per-symbol-variable registers (CFTW, ACR, CPOW).  The
 * static registers (CSR, CFR) are written once on init / mode change
 * via Encoder_WriteStaticRegs() and never in the ISR.
 */
typedef struct {
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
void Encoder_WriteStaticRegs(SPI_HandleTypeDef *hspi, uint8_t channel_mask);

/**
 * @brief  Pre-encode data registers (CFTW+ACR+CPOW) for TIM8 ISR replay.
 *
 * Unlike Encoder_FormatCommand(), this does NOT bit-interleave for
 * 2-bit serial mode.  Use AD9959 one-bit (default) serial mode only.
 *
 * @param  cmd   : DDS_Command with all fields populated
 * @param  frame : [out] pre-encoded byte arrays
 */
void Encoder_Encode1Bit(const DDS_Command *cmd, DDS_EncodedFrame *frame);

/* ---- 2-bit serial mode (original, kept for reference) ---- */

/**
 * @brief  Encode a DDS_Command into a multi-register SPI frame (2-bit mode).
 *
 * Frame format (per lane, interleaved from raw bytes):
 *   [0] CSR instruction (0x00)
 *   [1] CSR data = CSR_CHANNEL(profile) | CSR_IO_MODE_2BIT
 *   [2] CFTW instruction (0x04)
 *   [3:6] CFTW[31:0] MSB-first
 *   [7] ACR instruction (0x06)
 *   [8:10] ACR[23:0] = ramp_rate(0) | AMP_MULT_ENABLE | ASF[9:0]
 *   [11] CPOW instruction (0x05)
 *   [12:13] CPOW[15:0] = 0[1:0] | POW[13:0]
 *
 * @param  cmd        : DDS_Command with all fields populated
 * @param  spi1_frame : output buffer for SPI1 MOSI (>= ENCODER_FRAME_BYTES)
 * @param  spi3_frame : output buffer for SPI3 MOSI (>= ENCODER_FRAME_BYTES)
 * @return            : ENCODER_FRAME_BYTES (14)
 */
int Encoder_FormatCommand(const DDS_Command *cmd,
                          uint8_t *spi1_frame, uint8_t *spi3_frame);

#endif /* __DDS_ENCODER_H__ */
