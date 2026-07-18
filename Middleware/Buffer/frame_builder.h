/**
 ******************************************************************************
 * @file    frame_builder.h
 * @brief   SPI Frame Builder — Middleware layer buffer generation
 *
 * Thin wrappers over Modulator + Encoder.
 * Called from DMA ISR context to refill freed ping-pong bank.
 *
 * Frame length constraint:
 *   MAX bytes = FRAME_TRANSFER_TIME_US × (SPI baud / 8 bits)
 *   SPI baud = 125 Mbps, so 1 byte ≈ 4 SCLK × 8 ns = 32 ns.
 *
 *   The encoder output size determines actual frame_len.
 ******************************************************************************
 */

#ifndef __FRAME_BUILDER_H__
#define __FRAME_BUILDER_H__

#include <stdint.h>
#include "dds_command.h"
#include "mod_fsk.h"
#include "mod_ask.h"

/* ================================================================
 * Frame timing constraint (from phase1_config.h, default here)
 * ================================================================ */
#ifndef FRAME_TRANSFER_TIME_US
#define FRAME_TRANSFER_TIME_US    5U    /* 5 µs target transfer time */
#endif

/* Max bytes per frame given SPI 125 Mbps, 4-bit DataSize:
 *   1 byte = 4 SCLK × 8 ns = 32 ns
 *   max = FRAME_TRANSFER_TIME_US × 1000 / 32
 *        = 5 × 1000 / 32 ≈ 156 bytes  (plenty for single command) */
#define FRAME_MAX_BYTES  ((FRAME_TRANSFER_TIME_US * 1000UL) / 32)

/* ================================================================
 * Buffer-fill API (ISR-safe, no blocking calls)
 * ================================================================ */

/**
 * @brief  Build a CW SPI frame into the given buffer pair.
 * @return frame length (bytes per SPI lane)
 */
int FrameBuilder_CW(uint8_t *spi1_buf, uint8_t *spi3_buf,
                    uint32_t ftw, uint16_t asf, uint8_t profile);

/**
 * @brief  Build a single FSK-bit SPI frame.
 */
int FrameBuilder_FSK(uint8_t *spi1_buf, uint8_t *spi3_buf,
                     const FSK_Config *cfg, uint8_t bit);

/**
 * @brief  Build a single ASK-bit SPI frame.
 */
int FrameBuilder_ASK(uint8_t *spi1_buf, uint8_t *spi3_buf,
                     const ASK_Config *cfg, uint8_t bit);

#endif /* __FRAME_BUILDER_H__ */
