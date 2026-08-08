/**
 ******************************************************************************
 * @file    bsp_tim.c
 * @brief   BSP TIM abstraction — TIM4/8/15 + LPTIM3 for AD9959 timing chain
 *
 * Clock frequencies (from CubeMX RCC config):
 *   TIM4  (APB1) = 135 MHz
 *   TIM8  (APB2) = 281.25 MHz (PCLK2×2, APB2 presc≠1)
 *   TIM15 (APB2) = 281.25 MHz
 *   LPTIM3 (APB) = 135 MHz (DIV1)
 ******************************************************************************
 */

#include "bsp_tim.h"
#include "led_indicator.h"

/* APB2 timer clock from CubeMX RCC (SYSCLK / HPRE / D2PPRE2 × 2).
 * Local constant — BSP must not depend on DDS business headers. */
#define BSP_TIM_APB2_TIM_HZ  281250000UL

extern TIM_HandleTypeDef htim4;
extern TIM_HandleTypeDef htim8;
extern TIM_HandleTypeDef htim15;

/* ================================================================
 * TIM2 — removed from trigger chain (LPTIM3 replaces it as DMA sync
 * source).  Old code retained for reference only.
 * ================================================================ */

#if 0
void BSP_TIM2_SetFreq(uint32_t freq_hz)
{
    uint32_t arr = (256000000UL / freq_hz) - 1;
    if (arr < 2) arr = 2;
    __HAL_TIM_SET_AUTORELOAD(&htim2, arr);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, arr / 2);
}

void BSP_TIM2_Start(void)
{
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
}

void BSP_TIM2_Stop(void)
{
    HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
}
#endif /* TIM2 removed */

/* ================================================================
 * TIM4 — CS Timing Capture
 * ================================================================ */

void BSP_TIM4_Start(void)
{
    TIM4->SR = 0U;
    TIM4->CNT = 0U;
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
    if (cs_start) *cs_start = TIM4->CCR1;
    if (cs_end)   *cs_end   = TIM4->CCR2;
    __HAL_TIM_CLEAR_FLAG(&htim4,
                         TIM_FLAG_CC1 | TIM_FLAG_CC2 |
                         TIM_FLAG_CC1OF | TIM_FLAG_CC2OF);
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
    /* DMA mode: TIM8 is reset by LPTIM3_OUT via PA0 (TIM8_ETR). Direct
     * one-bit bring-up does not use TIM8 and leaves this state untouched. */
    TIM8->CR1 &= ~(TIM_CR1_CEN | TIM_CR1_OPM);
    TIM8->SMCR = TIM_SLAVEMODE_RESET | TIM_TS_ETRF;
    TIM8->PSC = 0U;
    TIM8->ARR = 0xFFFFU;
    TIM8->CNT = 0U;
    TIM8->EGR = TIM_EGR_UG;
    TIM8->SR = 0U;
    TIM8->BDTR |= TIM_BDTR_MOE;

    /* PWM2: low after the ETR reset (LPTIM3_OUT rising edge on PA0), then
     * rising at the calibrated CCR1/CCR2 delays.  The following reset is
     * the falling edge for the preceding synchronous pulse. */
    TIM8->CCMR1 &= ~(TIM_CCMR1_OC1M | TIM_CCMR1_OC1M_3);
    TIM8->CCMR1 |= TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_0;

    HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_2);
}

void BSP_TIM8_StartFreeRun(uint32_t freq_hz)
{
    /* Disable counter before reconfiguration. */
    TIM8->CR1 &= ~TIM_CR1_CEN;

    /* Keep CubeMX PWM config on CH1/CH2 (CCMR1, CCER, BDTR).
     * Only adjust period and ensure free-running (no slave mode). */
    TIM8->SMCR = 0U;
    TIM8->PSC  = 0U;
    uint32_t arr = (BSP_TIM_APB2_TIM_HZ / freq_hz) - 1;
    if (arr > 0xFFFFU) arr = 0xFFFFU;
    TIM8->ARR = (uint16_t)arr;
    TIM8->CCR1 = 500U;   /* PWM1: ~18 ns positive pulse for IO_UPDATE */
    TIM8->CNT = 0U;
    TIM8->EGR = TIM_EGR_UG;
    TIM8->SR  = 0U;

    /* Enable CH1 output (IO_UPDATE on PC6).  CubeMX configures the
     * channel but does not start it — HAL_TIM_PWM_Start is skipped.
     * TIM8 is an advanced timer: MOE is required for any pin output. */
    TIM8->BDTR |= TIM_BDTR_MOE;
    TIM8->CCER |= TIM_CCER_CC1E;

    /* Enable auto-reload preload and update interrupt.
     * CH1 PWM runs continuously — hardware IO_UPDATE at every period. */
    TIM8->CR1 |= TIM_CR1_ARPE;
    TIM8->DIER |= TIM_IT_UPDATE;

    /* Start counter. */
    TIM8->CR1 |= TIM_CR1_CEN;
}

void BSP_TIM8_Stop(void)
{
    TIM8->CR1 &= ~TIM_CR1_CEN;
    TIM8->DIER &= ~TIM_IT_UPDATE;
    HAL_TIM_PWM_Stop(&htim8, TIM_CHANNEL_1);
    HAL_TIM_PWM_Stop(&htim8, TIM_CHANNEL_2);
}

/* ================================================================
 * TIM15 — REF_CLK for AD9959 (CH1 PWM)
 * ================================================================ */

void BSP_TIM15_SetREFCLK(uint32_t freq_hz)
{
    /* Round to the nearest integer divider.  For 25 MHz with
     * APB2 Timer = 281.25 MHz → divider = 11 → 25.57 MHz. */
    uint32_t divider = (BSP_TIM_APB2_TIM_HZ + (freq_hz / 2UL)) / freq_hz;
    if (divider < 2UL) divider = 2UL;
    uint32_t arr = divider - 1UL;
    __HAL_TIM_SET_AUTORELOAD(&htim15, arr);
    __HAL_TIM_SET_COMPARE(&htim15, TIM_CHANNEL_1, (arr + 1UL) / 2UL);
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
    (void)htim;
}
/* HAL_TIM_PeriodElapsedCallback moved to main.c for combined TIM5+TIM8 handling. */

/* ================================================================
 * LPTIM3 — DMA Sync Gate
 *
 * CubeMX config: APB clock source (135 MHz), prescaler DIV1.
 * Period = LPTIM_ARR → pulse rate = 135 MHz / (ARR + 1).
 * Output (PA1) drives DMAMUX sync gate + TIM8_ETR + TIM4_ETR via fly-wire.
 * ================================================================ */

extern LPTIM_HandleTypeDef hlptim3;
static uint16_t lptim3_period = 0;

/* LPTIM3 kernel clock from CubeMX (RCC.LPTIM345Freq_Value). */
#define BSP_LPTIM3_TIM_HZ  135000000UL

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

/**
 * @brief  Compute LPTIM3 ARR from a desired pulse rate (Hz).
 */
uint16_t BSP_LPTIM3_ARRForRate(uint32_t rate_hz)
{
    if (rate_hz == 0U) return 0xFFFFU;
    uint32_t arr = (BSP_LPTIM3_TIM_HZ / rate_hz) - 1U;
    if (arr > 0xFFFFU) arr = 0xFFFFU;
    return (uint16_t)arr;
}
