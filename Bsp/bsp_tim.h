#ifndef __BSP_TIM_H__
#define __BSP_TIM_H__

#include "main.h"

/* ================================================================
 * TIM Callbacks (HAL weak overrides)
 * ================================================================ */

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim);
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);

/* ================================================================
 * TIM2 — DMA Trigger (CH1 PWM)
 * ================================================================ */

/**
 * @brief  Set TIM2 CH1 PWM frequency (determines DMA trigger rate)
 * @param  freq_hz  : trigger frequency in Hz
 * @note   TIM2 clock = 256 MHz (APB2 timer clock)
 */
void BSP_TIM2_SetFreq(uint32_t freq_hz);

/**
 * @brief  Start TIM2 PWM output (begins DMA trigger generation)
 */
void BSP_TIM2_Start(void);

/**
 * @brief  Stop TIM2 PWM output
 */
void BSP_TIM2_Stop(void);

/* ================================================================
 * TIM4 — CS Timing Capture (CH1/CH2 Input Capture + ETR Reset)
 * ================================================================ */

/**
 * @brief  Enable TIM4 input capture (CH1: CS↓, CH2: CS↑)
 * @note   TIM4 CNT is hardware-reset by ETR (connected to TIM2_CH1 via PE0)
 */
void BSP_TIM4_Start(void);

/**
 * @brief  Stop TIM4 capture
 */
void BSP_TIM4_Stop(void);

/**
 * @brief  Get the latest captured values
 * @param  cs_start  : CNT value at CS falling edge (TIM4_CH1)
 * @param  cs_end    : CNT value at CS rising edge (TIM4_CH2)
 */
void BSP_TIM4_GetCaptures(uint32_t *cs_start, uint32_t *cs_end);

/* ================================================================
 * TIM8 — IO_UPDATE + DIO3 pulse (CH1/CH2 Output Compare)
 * ================================================================ */

/**
 * @brief  Set TIM8 CH compare value (delay from TIM8 reset to pulse output)
 * @param  channel  : TIM_CHANNEL_1 (IO_UPDATE) or TIM_CHANNEL_2 (DIO3)
 * @param  delay_ticks : delay in timer clock ticks
 * @note   TIM8 is reset by TIM2 Update via ITR1 (SlaveMode=RESET)
 */
void BSP_TIM8_SetDelay(uint8_t channel, uint16_t delay_ticks);

/**
 * @brief  Enable TIM8 output compare channels, ready to receive ITR1 reset
 */
void BSP_TIM8_Start(void);

/**
 * @brief  Stop TIM8
 */
void BSP_TIM8_Stop(void);

/* ================================================================
 * TIM15 — REF_CLK for AD9959 (CH1 PWM)
 * ================================================================ */

/**
 * @brief  Set AD9959 reference clock frequency via TIM15 CH1 PWM
 * @param  freq_hz  : desired REF_CLK frequency (typically 25–50 MHz)
 * @note   TIM15 clock = 270 MHz, PSC = 0. 25 MHz request produces
 *         24.545 MHz (ARR = 10), the nearest integer division.
 *         若需要 50 MHz REF_CLK: freq_hz = 50000000
 */
void BSP_TIM15_SetREFCLK(uint32_t freq_hz);

/**
 * @brief  Start REF_CLK output
 */
void BSP_TIM15_Start(void);

/* ================================================================
 * LPTIM1 — DMA Sync Gate (OUT signal to DMAMUX)
 * ================================================================ */

/**
 * @brief  Set LPTIM1 auto-reload period
 * @param  period  : ARR value (LPTIM1 clock = 4 MHz after /32 prescaler)
 * @note   Period match generates LPTIM1_OUT → DMAMUX sync gate open
 */
void BSP_LPTIM1_SetPeriod(uint16_t period);

/**
 * @brief  Start LPTIM1 free-running counter with given period
 * @param  period  : if 0, use previously set value
 */
void BSP_LPTIM1_Start(uint16_t period);

/**
 * @brief  Stop LPTIM1 counter
 */
void BSP_LPTIM1_Stop(void);

#endif /* __BSP_TIM_H__ */
