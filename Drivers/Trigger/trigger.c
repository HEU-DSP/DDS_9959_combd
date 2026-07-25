/**
 ******************************************************************************
 * @file    trigger.c
 * @brief   LPTIM1 -> DMAMUX -> SPI DMA -> TIM8 trigger chain.
 ******************************************************************************
 */

#include "trigger.h"
#include "bsp_tim.h"
#include "bsp_spi.h"
#include "tx_buffer.h"
#include "phase1_config.h"
#include "debug_state.h"
#include <string.h>

static Trigger_Config trig_cfg;

void Trigger_Init(const Trigger_Config *cfg)
{
    memcpy((void *)&trig_cfg, cfg, sizeof(Trigger_Config));

    tx_active = 0;

    BSP_TIM2_SetFreq(cfg->sample_rate);

    uint16_t lptim_period = (16000000UL / cfg->sample_rate) - 1;
    BSP_LPTIM1_SetPeriod(lptim_period);

    BSP_TIM8_SetDelay(TIM_CHANNEL_1, cfg->ch1_delay);
    BSP_TIM8_SetDelay(TIM_CHANNEL_2, cfg->ch2_delay);
    ad9959_diag.tim8_ccr1 = TIM8->CCR1;
    ad9959_diag.tim8_ccr2 = TIM8->CCR2;

#if AD9959_TIM4_CALIBRATION_ENABLE
    BSP_TIM4_Start();
#endif
    BSP_TIM8_Start();
}

void Trigger_Start(void)
{
    ad9959_diag.dma_frame_bytes = trig_cfg.bank_size;
    __disable_irq();
    LPTIM1->CNT = 0;
    TIM2->CNT   = 0;
    __enable_irq();

    BSP_SPI_Both_DMA_Start(trig_cfg.spi1_ping, trig_cfg.spi3_ping,
                           trig_cfg.bank_size);

    BSP_LPTIM1_Start(0);
    BSP_TIM2_Start();
}

void Trigger_Stop(void)
{
    BSP_LPTIM1_Stop();
    BSP_TIM2_Stop();
    BSP_TIM8_Stop();
#if AD9959_TIM4_CALIBRATION_ENABLE
    BSP_TIM4_Stop();
#endif
    BSP_SPI_Both_Abort_IT();
}

void Trigger_Restart(void)
{
    __disable_irq();
    LPTIM1->CNT = 0;
    TIM2->CNT   = 0;
    __enable_irq();

    FrameBank *active = TxBuf_GetActive();
    ad9959_diag.dma_frame_bytes = tx_bank_bytes;
    BSP_SPI_Both_DMA_Start(active->spi1, active->spi3, tx_bank_bytes);

#if AD9959_TIM4_CALIBRATION_ENABLE
    BSP_TIM4_Start();
#endif
    BSP_TIM8_Start();
    BSP_LPTIM1_Start(0);
    BSP_TIM2_Start();
}

void Trigger_SwapBuffer(void)
{
    TxBuf_Swap();
    FrameBank *active = TxBuf_GetActive();
    ad9959_diag.dma_frame_bytes = tx_bank_bytes;

    BSP_SPI_Both_DMA_Start(active->spi1, active->spi3, tx_bank_bytes);
}

void Trigger_GetCaptureTiming(uint32_t *t_cs_start, uint32_t *t_cs_end)
{
    BSP_TIM4_GetCaptures(t_cs_start, t_cs_end);
}
