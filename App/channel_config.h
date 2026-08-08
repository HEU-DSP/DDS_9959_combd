/**
 ******************************************************************************
 * @file    channel_config.h
 * @brief   Middleware encoder: mod_cfg + sym_buf → pre-encoded DDS frames
 *
 * Encoder_BuildBank() is called from the main loop (or TIM8 ISR in
 * downgrade mode).  It reads global mod_cfg[4] and sym_buf, pre-encodes
 * DDS_EncodedFrame structs into pre_encoded[], ready for single-wire
 * SPI replay in the TIM8 periodic ISR.
 ******************************************************************************
 */

#ifndef __CHANNEL_CONFIG_H__
#define __CHANNEL_CONFIG_H__

#include <stdint.h>
#include <stdbool.h>
#include "mod_config.h"
#include "dds_encoder.h"

/** Pre-encoded frames, one per channel, for TIM8 ISR replay. */
extern DDS_EncodedFrame pre_encoded[DDS_CHANNEL_COUNT];
extern uint8_t pre_encoded_mask;  /* bitmask of channels with valid frames */

/**
 * @brief  Build pre-encoded DDS frames from mod_cfg + sym_buf.
 * @return true if at least one channel encoded, false if no active config.
 */
bool Encoder_BuildBank(void);

/**
 * @brief  DMA mode: encode all active channels directly into the idle
 *         FrameBank.spi1[] (zero-copy, no DDS_EncodedFrame intermediate).
 * @return total bytes written into the DMA buffer, 0 if none.
 * @note   Compiled only when AD9959_DOWNGRADE_MODE == 0.
 */
uint16_t Encoder_BuildBank_DMA(void);

#endif /* __CHANNEL_CONFIG_H__ */
