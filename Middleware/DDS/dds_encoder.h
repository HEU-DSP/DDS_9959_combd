/**
 ******************************************************************************
 * @file    dds_encoder.h
 * @brief   DDS_Command → SPI Frame Encoder
 *
 * Converts logical DDS_Command structures into byte sequences that
 * the SPI hardware transmits to AD9959.
 *
 * For 2-wire mode: needs to determine bit/byte assignment between
 * SPI1 (SDIO0) and SPI3 (SDIO1).
 *
 * Phase 1: both SPI lanes carry identical data (2-wire redundant mode).
 * Registers use CSR[7:4] for channel selection (per datasheet Table 29 fn 1).
 ******************************************************************************
 */

#ifndef __DDS_ENCODER_H__
#define __DDS_ENCODER_H__

#include "dds_command.h"

/**
 * @brief  Encode a DDS_Command into SPI frame bytes.
 *
 * Generates two byte sequences: one for SPI1 (SDIO0), one for SPI3 (SDIO1).
 * Each frame contains: [instruction byte | register data bytes].
 *
 * @param  cmd        : pointer to DDS_Command
 * @param  spi1_frame : output buffer for SPI1 MOSI data (must be >= 32 bytes)
 * @param  spi3_frame : output buffer for SPI3 MOSI data (must be >= 32 bytes)
 * @return            : number of bytes in each frame
 */
int Encoder_FormatCommand(const DDS_Command *cmd,
                          uint8_t *spi1_frame, uint8_t *spi3_frame);

/**
 * @brief  Encode an array of DDS_Commands into consecutive SPI frames
 * @return total bytes written per SPI lane
 */
int Encoder_FormatCommands(const DDS_Command *cmds, int num_cmds,
                           uint8_t *spi1_frame, uint8_t *spi3_frame);

#endif /* __DDS_ENCODER_H__ */
