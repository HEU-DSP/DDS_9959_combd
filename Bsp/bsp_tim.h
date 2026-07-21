#ifndef __BSP_TIM_H__
#define __BSP_TIM_H__

#include "main.h"

/* ================================================================
 * TIM Callbacks (HAL weak overrides)
 * ================================================================ */

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim);
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);

/* ================================================================
 * TIM4 — CS Timing Capture (CH1/CH2 Input Capture + ETR Reset)
 * ================================================================ */

/**
 * @brief  Enable TIM4 input capture (CH1: CS↓, CH2: CS↑)
 * @note   TIM4 CNT is hardware-reset by ETR (PE0 ← LPTIM3_OUT fly-wire)
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
 * @note   TIM8 is reset by ETR (PA0 ← LPTIM3_OUT fly-wire) via SlaveMode=RESET
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

/* ================================================================
 * TIM15 — REF_CLK for AD9959 (CH1 PWM)
 * ================================================================ */

/**
 * @brief  Set AD9959 reference clock frequency via TIM15 CH1 PWM
 * @param  freq_hz  : desired REF_CLK frequency (typically 25–50 MHz)
 * @note   TIM15 clock = 135 MHz (APB2 timer clock, PLL1=270MHz /2)
 *         若需要 50 MHz REF_CLK: freq_hz = 50000000
 */
void BSP_TIM15_SetREFCLK(uint32_t freq_hz);

/**
 * @brief  Start REF_CLK output
 */
void BSP_TIM15_Start(void);

/* ================================================================
 * LPTIM3 — Master Trigger (OUT signal → DMAMUX sync + TIM8 ETR + TIM4 ETR)
 * ================================================================ */

/**
 * @brief  Set LPTIM3 auto-reload period
 * @param  period  : ARR value (LPTIM3 clock = 135 MHz, prescaler /1)
 * @note   Period match generates LPTIM3_OUT → DMAMUX sync gate opens,
 *         TIM8 ETR (PA0) and TIM4 ETR (PE0) reset via external fly-wires.
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

#endif /* __BSP_TIM_H__ */
