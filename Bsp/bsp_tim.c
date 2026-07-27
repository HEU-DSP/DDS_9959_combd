/**
 ******************************************************************************
 * @file    bsp_tim.c
 * @brief   BSP TIM abstraction — TIM2/4/8/15 for AD9959 timing chain
 *
 * Clock frequencies (from CubeMX RCC config):
 *   TIM2  (APB2) = 256 MHz
 *   TIM4  (APB1) = 128 MHz
 *   TIM8  (APB2) = 256 MHz
 *   TIM15 (APB2) = 256 MHz
 ******************************************************************************
 */

#include "bsp_tim.h"
#include "led_indicator.h"

extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim4;
extern TIM_HandleTypeDef htim8;
extern TIM_HandleTypeDef htim15;

/* ================================================================
 * TIM2 — DMA Trigger (CH1 PWM, TRGO=UPDATE → ITR1 → TIM8)
 * ================================================================ */

void BSP_TIM2_SetFreq(uint32_t freq_hz)
{
    /* CubeMX MX_TIM2_Init() already configures:
     *   SlaveMode=RESET, Trigger=ETRF, ETR remap=COMP1
     *   MasterSlaveMode=ENABLE, TRGO=UPDATE
     * Here we only adjust the PWM frequency via ARR/CCR1. */
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
    /* Configure the continuous DMA synchronisation chain. Direct one-bit
     * bring-up does not use TIM8 and leaves this state untouched. */
    TIM8->CR1 &= ~(TIM_CR1_CEN | TIM_CR1_OPM);
    /* LPTIM3_OUT is externally fanned out to TIM8_ETR (PA0). */
    TIM8->SMCR = TIM_SLAVEMODE_RESET | TIM_TS_ETRF;
    TIM8->PSC = 0U;
    TIM8->ARR = 0xFFFFU;
    TIM8->CNT = 0U;
    TIM8->EGR = TIM_EGR_UG;
    TIM8->SR = 0U;
    TIM8->BDTR |= TIM_BDTR_MOE;

    /* Buffered/DMA mode uses PWM2: low after a TIM2 reset, then rising at
     * the calibrated CCR1 delay. */
    TIM8->CCMR1 &= ~(TIM_CCMR1_OC1M | TIM_CCMR1_OC1M_3);
    TIM8->CCMR1 |= TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_0;

    /* PWM2: each TIM2 update resets CNT and drives the pins low; CH1/CH2
     * rise at their calibrated CCR values.  The following reset is the
     * falling edge for the preceding synchronous pulse. */
    HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_2);
}

void BSP_TIM8_Stop(void)
{
    HAL_TIM_PWM_Stop(&htim8, TIM_CHANNEL_1);
    HAL_TIM_PWM_Stop(&htim8, TIM_CHANNEL_2);
}

/* ================================================================
 * TIM15 — REF_CLK for AD9959 (CH1 PWM)
 * ================================================================ */

void BSP_TIM15_SetREFCLK(uint32_t freq_hz)
{
    /* Round to the nearest integer divider. For a 25 MHz request this
     * selects /11, so TIM15 CH1 is 270 / 11 = 24.545 MHz (ARR = 10). */
    uint32_t divider = (270000000UL + (freq_hz / 2UL)) / freq_hz;
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

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM5) {
        Led_Refresh();
    }
}

/* ================================================================
 * LPTIM1 — DMA Sync Gate (16 MHz after /8 prescaler)
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
