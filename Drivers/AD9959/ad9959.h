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

/* One-bit serial readback values, retained for inspection in Ozone. */
typedef struct {
    volatile uint32_t marker;
    volatile uint32_t fr1;
    volatile uint32_t cftw;
    volatile uint32_t acr;
    volatile uint8_t csr;
    volatile uint8_t cfr[3];
} AD9959_OneBitDebug;

extern AD9959_OneBitDebug ad9959_onebit_debug;

/* Hardware-SPI transaction results, retained for inspection in Ozone. */
typedef struct {
    volatile uint32_t marker;
    volatile uint32_t transaction_count;
    volatile uint32_t software_spi;
    volatile uint32_t spi1_error;
    volatile uint32_t fr1_status;
    volatile uint32_t csr_status;
    volatile uint32_t cfr_status;
    volatile uint32_t cftw_status;
    volatile uint32_t acr_status;
    volatile uint32_t cpow_status;
} AD9959_HardwareSpiDebug;

extern AD9959_HardwareSpiDebug ad9959_hwspi_debug;

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
 * @note   Call HC595_Init() before this.
 */
void AD9959_Init(void);

/** Configure one channel for continuous sine-wave output during bring-up. */
void AD9959_SetCWDirect(uint8_t channel, uint32_t ftw, uint16_t asf);

/** Configure all four channels for the same continuous sine-wave output during bring-up. */
void AD9959_SetCWAllDirect(uint32_t ftw, uint16_t asf);
void AD9959_SetCWOfficialPerChannel(uint32_t ftw, uint16_t asf);
void AD9959_SetCWOfficialChannel(uint8_t channel, uint32_t ftw, uint16_t asf);

/** Switch AD9959 CHx from the reset-default single-bit bus to 2-bit serial
 * mode, then reconfigure SPI1/SPI3 to 4-bit DMA transfers. */
bool AD9959_Enable2BitSerial(uint8_t channel);

/* Return PC6/PC7 to TIM8_CH1/CH2 after the one-bit GPIO initialization has
 * finished all of its low-frequency software IO_UPDATE pulses. */
void AD9959_EnableRuntimeTimerOutputs(void);

void AD9959_CyclicFTW_Start(uint32_t ftw, uint32_t period_ms);
void AD9959_CyclicFTW_Task(void);
void AD9959_CyclicFTW_OnSpiTxComplete(void);
void AD9959_CyclicFTW_OnSpiError(void);

/**
 * @brief  Trigger one GPIO IO_UPDATE pulse for direct one-bit bring-up.
 */
void AD9959_IOUpdate(void);

/** Hold DDS reset high and continuously send a known SPI1 waveform. */
void AD9959_DebugSpiWaveformTest(void);

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

#endif /* __AD9959_H__ */
