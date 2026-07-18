/**
 ******************************************************************************
 * @file    trigger.c
 * @brief   LPTIM1→DMA→SPI→TIM8 Trigger Chain Implementation
 *
 * CubeMX hardware config:
 *   LPTIM1: /8 prescaler → 16 MHz, OUT signal → DMAMUX Sync for SPI DMA
 *   TIM2:   SlaveMode=RESET on ETRF (COMP1 remap), TRGO=Update → ITR1→TIM8
 *   TIM4:   SlaveMode=RESET on ETRF (PE0 ← PA0 wire), CH1/CH2 = IC
 *   TIM8:   SlaveMode=RESET on ITR1 (TIM2), CH1/CH2 = OC Timing
 *   DMA:    SPI1_TX (Stream2) + SPI3_TX (Stream1), Sync=LPTIM1_OUT, Normal
 ******************************************************************************
 */

#include "trigger.h"
#include "bsp_tim.h"
#include "bsp_spi.h"
#include "tx_buffer.h"
#include <string.h>

static Trigger_Config trig_cfg;
static volatile uint8_t active_bank;   /* 0=ping, 1=pong */

/* ================================================================
 * Init
 * ================================================================ */

void Trigger_Init(const Trigger_Config *cfg)
{
    memcpy((void *)&trig_cfg, cfg, sizeof(Trigger_Config));
    active_bank = 0;

    /* 1. TIM2 PWM frequency (ARR/CCR1 only, slave config in MX_TIM2_Init) */
    BSP_TIM2_SetFreq(cfg->sample_rate);

    /* 2. LPTIM1 period: 16 MHz / sample_rate - 1 */
    uint16_t lptim_period = (16000000UL / cfg->sample_rate) - 1;
    BSP_LPTIM1_SetPeriod(lptim_period);

    /* 3. TIM8 OC delays */
    BSP_TIM8_SetDelay(TIM_CHANNEL_1, cfg->ch1_delay);
    BSP_TIM8_SetDelay(TIM_CHANNEL_2, cfg->ch2_delay);

    /* 4. TIM4 capture enabled */
    BSP_TIM4_Start();

    /* 5. TIM8 OC enabled */
    BSP_TIM8_Start();

}

/* ================================================================
 * Start
 * ================================================================ */

void Trigger_Start(void)
{
    /* Phase-align LPTIM1 and TIM2: both counters → 0 */
    __disable_irq();
    LPTIM1->CNT = 0;
    TIM2->CNT   = 0;
    __enable_irq();

    /* Start both timers simultaneously */
    BSP_LPTIM1_Start(0);   /* use previously set period */
    BSP_TIM2_Start();      /* PWM on CH1 */

    /* ARM DMA channels — both wait for LPTIM1_OUT sync */
    BSP_SPI_Both_DMA_Start(trig_cfg.spi1_ping, trig_cfg.spi3_ping,
                           trig_cfg.bank_size);
}

/* ================================================================
 * Stop
 * ================================================================ */

void Trigger_Stop(void)
{
    BSP_LPTIM1_Stop();
    BSP_TIM2_Stop();
    BSP_TIM8_Stop();
    BSP_TIM4_Stop();
    BSP_SPI_Both_Abort();
}

void Trigger_Restart(void)
{
    /* Reset phase alignment and re-start without full re-init */
    __disable_irq();
    LPTIM1->CNT = 0;
    TIM2->CNT   = 0;
    __enable_irq();

    BSP_LPTIM1_Start(0);
    BSP_TIM2_Start();
    BSP_TIM4_Start();
    BSP_TIM8_Start();

    FrameBank *active = TxBuf_GetActive();
    BSP_SPI_Both_DMA_Start(active->spi1, active->spi3, tx_bank_bytes);
}

/* ================================================================
 * Buffer Swap (DMA TC ISR → this function)
 * ================================================================ */

void Trigger_SwapBuffer(void)
{
    active_bank ^= 1;

    const uint8_t *s1 = (active_bank == 0) ? trig_cfg.spi1_ping : trig_cfg.spi1_pong;
    const uint8_t *s3 = (active_bank == 0) ? trig_cfg.spi3_ping : trig_cfg.spi3_pong;

    /* Re-arm DMA for next bank */
    BSP_SPI_Both_DMA_Start(s1, s3, trig_cfg.bank_size);
}

/* ================================================================
 * Capture Readback
 * ================================================================ */

void Trigger_GetCaptureTiming(uint32_t *t_cs_start, uint32_t *t_cs_end)
{
    BSP_TIM4_GetCaptures(t_cs_start, t_cs_end);
}
