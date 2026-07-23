/**
 ******************************************************************************
 * @file    ad9959.h
 * @brief   AD9959 DDS Driver Interface
 ******************************************************************************
 */

#ifndef __AD9959_H__
#define __AD9959_H__

#include <stdint.h>
#include <stdbool.h>

/* ================================================================
 * AD9959 System Clock (REFCLK × PLL multiplier)
 * REFCLK = 25.6 MHz (TIM15), PLL ×19 → SYSCLK = 486.4 MHz
 * ================================================================ */
#define AD9959_SYSCLK_HZ   486400000UL

/* ================================================================
 * Initialization & Control
 * ================================================================ */

/**
 * @brief  Full AD9959 initialization:
 *         1. Power-up sequence via 595 (DVDD_1.8, AVDD_1.8, DVDD_3.3, REFCLK mode)
 *         2. Master Reset pulse
 *         3. Configure FR1 (PLL multiplication, VCO gain, charge pump)
 *         4. Configure FR2
 *         5. Enable REF_CLK output (TIM15)
 * @note   Call DRV_595_Init() before this.
 */
void AD9959_Init(void);
void AD9959_ConfigPLL(void);

/**
 * @brief  Trigger IO_UPDATE pulse via TIM8 CH1 hardware
 */
void AD9959_IOUpdate(void);

/**
 * @brief  Assert Master Reset (via 595 DDSMASTERRST bit)
 */
void AD9959_Reset(void);

/**
 * @brief  Power-down control (via 595 DDSPDN bit)
 */
void AD9959_PowerDown(bool enable);

/* ================================================================
 * Register Access (dual-wire: SPI1 SDIO0 + SPI3 SDIO1)
 * ================================================================ */

/**
 * @brief  Write to AD9959 register via dual-line SPI
 * @param  reg       : register address (AD9959_REG_xxx)
 * @param  data      : pointer to data bytes
 * @param  num_bytes : number of data bytes to write
 */
void AD9959_WriteRegister(uint8_t reg, const uint8_t *data, uint8_t num_bytes);

/**
 * @brief  Read from AD9959 register via SPI1 MISO (single-line read)
 * @param  reg       : register address | 0x80 (read bit)
 * @param  data      : output buffer
 * @param  num_bytes : number of bytes to read
 */
void AD9959_ReadRegister(uint8_t reg, uint8_t *data, uint8_t num_bytes);

/* ================================================================
 * High-Level Parameter Setting
 * ================================================================ */

/**
 * @brief  Set Frequency Tuning Word for a channel
 * @param  channel  : 0–3
 * @param  ftw      : 32-bit FTW
 *         FTW = f_out / f_sysclk * 2^32
 */
void AD9959_SetFTW(uint8_t channel, uint32_t ftw);

/**
 * @brief  Set Amplitude Scale Factor for a channel (via ACR)
 * @param  channel  : 0–3
 * @param  asf      : 10-bit amplitude (0 = off, 0x3FF = full scale)
 */
void AD9959_SetASF(uint8_t channel, uint16_t asf);

/**
 * @brief  Set Phase Offset Word for a channel
 * @param  channel  : 0–3
 * @param  pow      : 14-bit phase offset
 */
void AD9959_SetPOW(uint8_t channel, uint16_t pow);

/**
 * @brief  Enable/disable specific channels in CSR
 * @param  channel_mask : bitmask (CSR_CH0_ENABLE | CSR_CH1_ENABLE | ...)
 */
void AD9959_SetChannelMask(uint8_t channel_mask);

/**
 * @brief  Debug: configure CH0 for CW output at given frequency, max amplitude.
 * @param  freq_hz: output frequency in Hz (e.g. 200000000)
 * @note   Call after AD9959_Init(). Blocking SPI, single-wire mode.
 */
void AD9959_Debug_CW_Test(uint32_t freq_hz);

#endif /* __AD9959_H__ */
