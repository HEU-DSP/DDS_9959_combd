/**
 ******************************************************************************
 * @file    bsp_tim.c
 * @brief   BSP TIM abstraction — TIM4/8/15 + LPTIM3 for AD9959 timing chain
 *
 * Clock frequencies (from CubeMX RCC config):
 *   TIM4  (APB1)  = 135 MHz
 *   TIM8  (APB2)  = 135 MHz
 *   TIM15 (APB2) clock = HCLK = 256 MHz (HSE=12M, PLL1: /3×128, HCLK=/2)
 *   LPTIM3 (D3PCLK1) = 135 MHz, prescaler /1
 ******************************************************************************
 */

#include "bsp_tim.h"

extern TIM_HandleTypeDef htim4;
extern TIM_HandleTypeDef htim8;
extern TIM_HandleTypeDef htim15;

static volatile uint32_t tim4_cs_start = 0;
static volatile uint32_t tim4_cs_end   = 0;

/* ================================================================
 * TIM4 — CS Timing Capture
 * ================================================================ */

void BSP_TIM4_Start(void)
{
    HAL_TIM_IC_Start(&htim4, TIM_CHANNEL_1);
    HAL_TIM_IC_Start(&htim4, TIM_CHANNEL_2);
}

void BSP_TIM4_Stop(void)
{
    HAL_TIM_IC_Stop(&htim4, TIM_CHANNEL_1);
    HAL_TIM_IC_Stop(&htim4, TIM_CHANNEL_2);
}

void BSP_TIM4_GetCaptures(uint32_t *cs_start, uint32_t *cs_end)
{
    if (cs_start) *cs_start = tim4_cs_start;
    if (cs_end)   *cs_end   = tim4_cs_end;
}

/* ================================================================
 * TIM8 — IO_UPDATE + DIO3 Pulse Output
 * ================================================================ */

void BSP_TIM8_SetDelay(uint8_t channel, uint16_t delay_ticks)
{
    __HAL_TIM_SET_COMPARE(&htim8, channel, delay_ticks);
}

void BSP_TIM8_Start(void)
{
    HAL_TIM_OC_Start(&htim8, TIM_CHANNEL_1);
    HAL_TIM_OC_Start(&htim8, TIM_CHANNEL_2);
}

void BSP_TIM8_Stop(void)
{
    HAL_TIM_OC_Stop(&htim8, TIM_CHANNEL_1);
    HAL_TIM_OC_Stop(&htim8, TIM_CHANNEL_2);
}

/* ================================================================
 * TIM15 — REF_CLK for AD9959 (CH1 PWM)
 * ================================================================ */

void BSP_TIM15_SetREFCLK(uint32_t freq_hz)
{
    uint32_t tim_clk = HAL_RCC_GetHCLKFreq();   /* TIM15 clock == HCLK */
    uint32_t arr = (tim_clk / freq_hz) - 1;
    if (arr < 2) arr = 2;
    if (arr > 65535) arr = 65535;
    __HAL_TIM_SET_AUTORELOAD(&htim15, arr);
    __HAL_TIM_SET_COMPARE(&htim15, TIM_CHANNEL_1, (arr + 1) / 2);
}

void BSP_TIM15_Start(void)
{
    HAL_TIM_PWM_Start(&htim15, TIM_CHANNEL_1);
}

/* ================================================================
 * HAL Callbacks (weak overrides)
 * ================================================================ */

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM4) {
        if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {
            tim4_cs_start = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
        } else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2) {
            tim4_cs_end = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);
        }
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    (void)htim;
}

/* ================================================================
 * LPTIM3 — Master Trigger (135 MHz, prescaler /1)
 * OUT → DMAMUX sync gate + TIM8 ETR (PA0) + TIM4 ETR (PE0)
 * ================================================================ */

extern LPTIM_HandleTypeDef hlptim3;
static uint16_t lptim3_period = 0;

void BSP_LPTIM3_SetPeriod(uint16_t period)
{
    lptim3_period = period;
    hlptim3.Instance->ARR = period;
}

void BSP_LPTIM3_Start(uint16_t period)
{
    if (period > 0) {
        lptim3_period = period;
        hlptim3.Instance->ARR = period;
    }
    HAL_LPTIM_Counter_Start(&hlptim3, lptim3_period);
}

void BSP_LPTIM3_Stop(void)
{
    HAL_LPTIM_Counter_Stop(&hlptim3);
}
