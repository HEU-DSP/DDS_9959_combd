/**
 ******************************************************************************
 * @file    trigger.c
 * @brief   LPTIM3 -> DMAMUX -> SPI DMA -> TIM8 trigger chain.
 ******************************************************************************
 */

#include "trigger.h"
#include "bsp_tim.h"
#include "bsp_spi.h"
#include "tx_buffer.h"
#include "phase1_config.h"
#include "debug_state.h"
#include "dds_encoder.h"
#include <string.h>

/* CubeMX-generated handle for SPI1 TX DMA (DMAMUX sync reconfiguration). */
extern DMA_HandleTypeDef hdma_spi1_tx;

static Trigger_Config trig_cfg;

/**
 * @brief  Reconfigure the SPI1 TX DMAMUX sync gate for DMA mode.
 *
 * CubeMX generates RequestNumber = 1 (one DMA byte per LPTIM3_OUT pulse).
 * For a multi-register AD9959 frame, one pulse must unlock exactly one
 * channel's 14-byte register block; the DMA then pauses at the next
 * sync boundary until the following pulse.  REQNB is a 5-bit field
 * (max 32), so 14 per pulse is the natural fit.
 *
 * @note  Must be called while the DMA stream is disabled.
 */
static void trigger_setup_dmamux(void)
{
    HAL_DMA_MuxSyncConfigTypeDef sync_cfg;

    sync_cfg.SyncSignalID = HAL_DMAMUX1_SYNC_LPTIM3_OUT;
    sync_cfg.SyncPolarity = HAL_DMAMUX_SYNC_RISING;
    sync_cfg.SyncEnable   = ENABLE;
    sync_cfg.EventEnable  = DISABLE;
    sync_cfg.RequestNumber = ENCODER_FRAME_FLAT_BYTES;  /* 14 bytes per pulse */

    if (HAL_DMAEx_ConfigMuxSync(&hdma_spi1_tx, &sync_cfg) != HAL_OK) {
        /* Register write failure is fatal for the trigger chain. */
        ad9959_diag.dmamux1_csr = DMAMUX1_ChannelStatus->CSR;
    }
}

void Trigger_Init(const Trigger_Config *cfg)
{
    memcpy((void *)&trig_cfg, cfg, sizeof(Trigger_Config));

    tx_active = 0;

    /* LPTIM3 pulse rate = per-channel update rate (135 MHz / (ARR+1)). */
    BSP_LPTIM3_SetPeriod(BSP_LPTIM3_ARRForRate(cfg->sample_rate));

    /* One LPTIM3 pulse gates one channel (14 bytes) through the DMA. */
    trigger_setup_dmamux();

    BSP_TIM8_SetDelay(TIM_CHANNEL_1, cfg->ch1_delay);
    BSP_TIM8_SetDelay(TIM_CHANNEL_2, cfg->ch2_delay);
    ad9959_diag.tim8_ccr1 = TIM8->CCR1;
    ad9959_diag.tim8_ccr2 = TIM8->CCR2;

#if AD9959_TIM4_MONITOR_ENABLE
    BSP_TIM4_Start();
#endif
    BSP_TIM8_Start();
}

void Trigger_Start(void)
{
    ad9959_diag.dma_frame_bytes = trig_cfg.bank_size;
    __disable_irq();
    LPTIM3->CNT = 0;
    __enable_irq();

    BSP_SPI_Both_DMA_Start(trig_cfg.spi1_ping, trig_cfg.spi3_ping,
                           trig_cfg.bank_size);

    BSP_LPTIM3_Start(0);
}

void Trigger_Stop(void)
{
    BSP_LPTIM3_Stop();
    BSP_TIM8_Stop();
#if AD9959_TIM4_MONITOR_ENABLE
    BSP_TIM4_Stop();
#endif
    BSP_SPI_Both_Abort_IT();
}

void Trigger_Restart(void)
{
    __disable_irq();
    LPTIM3->CNT = 0;
    __enable_irq();

    FrameBank *active = TxBuf_GetActive();
    ad9959_diag.dma_frame_bytes = tx_bank_bytes;
    BSP_SPI_Both_DMA_Start(active->spi1, active->spi3, tx_bank_bytes);

#if AD9959_TIM4_MONITOR_ENABLE
    BSP_TIM4_Start();
#endif
    BSP_TIM8_Start();
    BSP_LPTIM3_Start(0);
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
