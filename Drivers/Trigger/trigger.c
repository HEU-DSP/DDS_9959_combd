/**
 ******************************************************************************
 * @file    trigger.c
 * @brief   LPTIM3→DMA→SPI→TIM8 Trigger Chain Implementation
 *
 * CubeMX hardware config:
 *   LPTIM3: /1 prescaler → 135 MHz, OUT → DMAMUX Sync for SPI DMA,
 *           + fly-wire PA0→TIM8 ETR, fly-wire PE0→TIM4 ETR
 *   TIM4:   SlaveMode=RESET on ETRF (PE0 ← LPTIM3_OUT wire), CH1/CH2 = IC
 *   TIM8:   SlaveMode=RESET on ETRF (PA0 ← LPTIM3_OUT wire), CH1/CH2 = OC Timing
 *   DMA:    SPI1_TX (Stream2) + SPI3_TX (Stream1), Sync=LPTIM3_OUT, Normal
 ******************************************************************************
 */

#include "trigger.h"
#include "bsp_tim.h"
#include "bsp_spi.h"
#include "tx_buffer.h"
#include <string.h>

static Trigger_Config trig_cfg;

/* ================================================================
 * Init
 * ================================================================ */

void Trigger_Init(const Trigger_Config *cfg)
{
    memcpy((void *)&trig_cfg, cfg, sizeof(Trigger_Config));

    /* tx_active is the single source of truth for ping-pong ownership:
     * DMA reads TxBuf_GetActive(), CPU fills TxBuf_GetIdle(). */
    tx_active = 0;

    /* 1. LPTIM3 period: 135 MHz / sample_rate - 1 */
    uint16_t lptim_period = (uint16_t)(135000000UL / cfg->sample_rate - 1);
    BSP_LPTIM3_SetPeriod(lptim_period);

    /* 2. TIM8 OC delays */
    BSP_TIM8_SetDelay(TIM_CHANNEL_1, cfg->ch1_delay);
    BSP_TIM8_SetDelay(TIM_CHANNEL_2, cfg->ch2_delay);

    /* 3. TIM4 capture enabled */
    BSP_TIM4_Start();

    /* 4. TIM8 OC enabled */
    BSP_TIM8_Start();
}

/* ================================================================
 * Start
 * ================================================================ */

void Trigger_Start(void)
{
    /* Phase-align LPTIM3: counter → 0 */
    __disable_irq();
    LPTIM3->CNT = 0;
    __enable_irq();

    /* Start LPTIM3 */
    BSP_LPTIM3_Start(0);   /* use previously set period */

    /* ARM DMA channels — both wait for LPTIM3_OUT sync */
    BSP_SPI_Both_DMA_Start(trig_cfg.spi1_ping, trig_cfg.spi3_ping,
                           trig_cfg.bank_size);
}

/* ================================================================
 * Stop
 * ================================================================ */

void Trigger_Stop(void)
{
    BSP_LPTIM3_Stop();
    BSP_TIM8_Stop();
    BSP_TIM4_Stop();
    BSP_SPI_Both_Abort();
}

void Trigger_Restart(void)
{
    /* Reset phase alignment and re-start without full re-init */
    __disable_irq();
    LPTIM3->CNT = 0;
    __enable_irq();

    BSP_LPTIM3_Start(0);
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
    TxBuf_Swap();
    FrameBank *active = TxBuf_GetActive();

    /* Re-arm DMA with the just-promoted bank. tx_bank_bytes may change after
     * App/Middleware refills the opposite bank, so use the live length. */
    BSP_SPI_Both_DMA_Start(active->spi1, active->spi3, tx_bank_bytes);
}

/* ================================================================
 * Capture Readback
 * ================================================================ */

void Trigger_GetCaptureTiming(uint32_t *t_cs_start, uint32_t *t_cs_end)
{
    BSP_TIM4_GetCaptures(t_cs_start, t_cs_end);
}
