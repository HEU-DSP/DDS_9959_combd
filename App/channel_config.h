/**
 ******************************************************************************
 * @file    channel_config.h
 * @brief   Middleware encoder: mod_cfg + sym_buf → SPI frames → tx_bank
 *
 * Encoder_BuildBank() is called from DMA ISR after a bank completes.
 * It reads global mod_cfg[4] and sym_buf, encodes SPI frames,
 * and fills the idle ping-pong bank.
 ******************************************************************************
 */

#ifndef __CHANNEL_CONFIG_H__
#define __CHANNEL_CONFIG_H__

#include <stdint.h>
#include <stdbool.h>
#include "tx_buffer.h"

/**
 * @brief  Build encoded SPI frames from mod_cfg + sym_buf into a bank.
 * @param  bank          : idle bank to fill
 * @param  out_bank_bytes: [out] total bytes written
 * @return true if frames were generated, false if no data (should STOP).
 */
bool Encoder_BuildBank(FrameBank *bank, uint16_t *out_bank_bytes);

#endif /* __CHANNEL_CONFIG_H__ */
