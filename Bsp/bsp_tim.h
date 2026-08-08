#ifndef __BSP_TIM_H__
#define __BSP_TIM_H__

#include "main.h"

/* ================================================================
 * TIM Callbacks (HAL weak overrides)
 * ================================================================ */

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim);
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);

/* ================================================================
 * TIM2 — removed from trigger chain (replaced by LPTIM3 sync).
 * Old declarations retained for reference in bsp_tim.c only.
 * ================================================================ */

/* ================================================================
 * TIM4 — CS Timing Capture (CH1/CH2 Input Capture + ETR Reset)
 * ================================================================ */

/**
 * @brief  Enable TIM4 input capture (CH1: CS↓, CH2: CS↑)
 * @note   TIM4 CNT is hardware-reset by ETR (PE0, fly-wired to LPTIM3_OUT)
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
 * @note   TIM8 is reset by LPTIM3_OUT via TIM8_ETR (PA0, SlaveMode=RESET)
 */
void BSP_TIM8_SetDelay(uint8_t channel, uint16_t delay_ticks);

/**
 * @brief  Enable TIM8 output compare channels, ready to receive ETR reset
 */
void BSP_TIM8_Start(void);

/**
 * @brief  Stop TIM8
 */
void BSP_TIM8_Stop(void);

/**
 * @brief  Configure TIM8 as free-running periodic timer (no slave/PWM).
 *
 * Used in downgrade mode: TIM8 generates a periodic update interrupt at
 * the requested frequency, driving single-wire AD9959 register sync.
 *
 * @param  freq_hz  : update interrupt frequency (e.g. 100000 → 100 kHz)
 * @note   TIM8 clock = 281.25 MHz (APB2), PSC = 0.
 *         ARR = 281 250 000 / freq_hz - 1.
 *         This overrides the old slave-mode + PWM configuration.
 */
void BSP_TIM8_StartFreeRun(uint32_t freq_hz);

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
 * LPTIM3 — DMA Sync Gate (OUT signal to DMAMUX)
 *
 * CubeMX config: APB clock (135 MHz), prescaler DIV1.
 * LPTIM3_OUT (PA1) → DMAMUX sync gate + TIM8_ETR (PA0) + TIM4_ETR (PE0).
 * ================================================================ */

/**
 * @brief  Set LPTIM3 auto-reload period
 * @param  period  : ARR value (LPTIM3 clock = 135 MHz, DIV1)
 * @note   Period match generates LPTIM3_OUT → DMAMUX sync gate open
 */
void BSP_LPTIM3_SetPeriod(uint16_t period);

/**
 * @brief  Start LPTIM3 free-running counter with given period
 * @param  period  : if 0, use previously set value
 */
void BSP_LPTIM3_Start(uint16_t period);

/**
 * @brief  Stop LPTIM3 counter
 */
void BSP_LPTIM3_Stop(void);

/**
 * @brief  Compute LPTIM3 ARR from a desired pulse rate (Hz).
 * @param  rate_hz : desired LPTIM3_OUT pulse rate
 * @return ARR value (clamped to 16-bit range)
 */
uint16_t BSP_LPTIM3_ARRForRate(uint32_t rate_hz);

#endif /* __BSP_TIM_H__ */
