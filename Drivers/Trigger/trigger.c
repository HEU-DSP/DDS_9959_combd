/**
 ******************************************************************************
 * @file    trigger.c
 * @brief   LPTIM3 -> DMAMUX -> SPI DMA -> TIM8 trigger chain.
 ******************************************************************************
 */

#include "trigger.h"
#include "bsp_tim.h"
#include "bsp_spi.h"
#include "bsp_dwt.h"
#include "tx_buffer.h"
#include "phase1_config.h"
#include "debug_state.h"
#include "dds_encoder.h"
#include <string.h>

/* CubeMX-generated handles (DMAMUX sync reconfiguration + idle polling). */
extern DMA_HandleTypeDef hdma_spi1_tx;
extern SPI_HandleTypeDef hspi1;

static Trigger_Config trig_cfg;

#define TRIGGER_WAIT_CHUNK_ITER   20000U   /* spins before yielding to DWT_Delay */
#define TRIGGER_WAIT_CHUNK_US     1U       /* µs slept per exhausted chunk       */

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

/**
 * @brief  Bounded poll until SPI1 is READY.
 *
 * One outer pass ≈ TRIGGER_WAIT_CHUNK_ITER × TRIGGER_WAIT_CHUNK_US.  A DMA
 * abort with a transfer genuinely in flight completes in a few microseconds,
 * so a whole pass finishing means something structural is wrong (e.g. the
 * abort callback never fired because the stream was already dead).
 *
 * @param  timeout_ms  total budget
 * @return HAL_OK if READY was reached, HAL_TIMEOUT otherwise.
 */
static HAL_StatusTypeDef trigger_wait_spi_ready(uint32_t timeout_ms)
{
    uint32_t passes = (timeout_ms + TRIGGER_WAIT_CHUNK_US - 1U)
                      / TRIGGER_WAIT_CHUNK_US;

    for (uint32_t pass = 0U; pass < passes; pass++) {
        for (uint32_t i = 0U; i < TRIGGER_WAIT_CHUNK_ITER; i++) {
            if (HAL_SPI_GetState(&hspi1) == HAL_SPI_STATE_READY) {
                return HAL_OK;
            }
        }
        DWT_Delay((float)TRIGGER_WAIT_CHUNK_US);
    }
    return HAL_TIMEOUT;
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

/**
 * @brief  Arm the DMA and only then start LPTIM3.
 *
 * The order matters: an armed-but-idle DMA that never receives its LPTIM3
 * pulses is harmless, while a running LPTIM3 with no armed DMA overruns the
 * DMAMUX sync gate — which HAL turns into a permanent chain stop.
 *
 * @return HAL_OK on success, HAL_ERROR if the DMA refused to arm (LPTIM3 is
 *         then left stopped).
 */
static HAL_StatusTypeDef trigger_arm_and_run(const uint8_t *buf)
{
    ad9959_diag.dma_frame_bytes = tx_bank_bytes;

    /* PRIMASK critical section, not __disable_irq().  HAL_DMA_Start_IT() and
     * HAL_SPI_Transmit_DMA() both re-arm a shared resource the SPI1 EOT and
     * DMA1_Stream1 ISRs manipulate, so LPTIM3->CNT, the arm and the LPTIM3
     * start must be one indivisible step with respect to them.  It also stops
     * HAL_SPI_ErrorCallback() from latching a stale chain_fault in the window
     * between a successful arm and the first LPTIM3 pulse.
     *
     * NOTE: main.c re-enables interrupts unconditionally after __disable_irq()
     * (flag-preserving restore is deliberately not used there), so enter this
     * with interrupts ENABLED — every caller does. */
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    LPTIM3->CNT = 0;
    HAL_StatusTypeDef status = BSP_SPI_Both_DMA_Start(buf, NULL, tx_bank_bytes);
    if (status == HAL_OK) {
        BSP_LPTIM3_Start(0);
    } else {
        ad9959_diag.tx_running = 0U;
    }

    if (primask == 0U) {
        __enable_irq();
    }
    return status;
}

HAL_StatusTypeDef Trigger_Start(void)
{
    /* Boot path: arm the DMA from tx_bank[0] with the live byte count.
     * tx_bank_bytes (not trig_cfg.bank_size) is the source of truth —
     * the cached copy goes stale after any APPLY that changes the
     * active channel count. */
#if AD9959_TIM4_MONITOR_ENABLE
    BSP_TIM4_Start();
#endif
    BSP_TIM8_Start();

    return trigger_arm_and_run(trig_cfg.spi1_ping);
}

/**
 * @brief  Stop the chain and wait until SPI1 is genuinely idle.
 *
 * BSP_SPI_Both_Abort_IT() (HAL_SPI_Abort_IT) is asynchronous: it only flags
 * SPI1 ABORT and the completion lands later in the DMA abort callback.
 * Anything that touches SPI1 — a blocking AD9959_WriteRegister, a follow-up
 * Trigger_Restart() — in that window gets HAL_BUSY, which used to be swallowed
 * silently (bsp_spi.c) and left LPTIM3 running against an un-armed DMA.
 *
 * @return HAL_OK once SPI1 (and its DMA stream) are idle, HAL_TIMEOUT if SPI1
 *         never settles.
 */
HAL_StatusTypeDef Trigger_Stop(void)
{
    BSP_LPTIM3_Stop();
    BSP_TIM8_Stop();
#if AD9959_TIM4_MONITOR_ENABLE
    BSP_TIM4_Stop();
#endif

    if (HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY) {
        BSP_SPI_Both_Abort_IT();
    }
    /* Always poll: an in-flight transfer can also be closing right now. */
    if (trigger_wait_spi_ready(TRIGGER_ABORT_TIMEOUT_MS) != HAL_OK) {
        return HAL_TIMEOUT;
    }

    /* SPI1 reading READY does NOT mean its DMA stream is idle.  On the sync
     * overrun path (SPI_DMAError → SPI_CloseTransfer) the SPI is closed and
     * TxXferCount cleared while the stream is left BUSY, and HAL_SPI_Transmit_DMA()
     * would then fail on HAL_DMA_Start_IT().  Parking the stream here is what
     * lets a later Trigger_Restart() actually bring the chain back. */
    BSP_SPI_ForceDmaIdle();

    return HAL_OK;
}

HAL_StatusTypeDef Trigger_Restart(void)
{
    /* Chain was stopped first, so SPI1 must be idle before re-arming. */
    if (trigger_wait_spi_ready(TRIGGER_ARM_TIMEOUT_MS) != HAL_OK) {
        ad9959_diag.tx_running = 0U;
        return HAL_BUSY;
    }

#if AD9959_TIM4_MONITOR_ENABLE
    BSP_TIM4_Start();
#endif
    BSP_TIM8_Start();

    return trigger_arm_and_run(TxBuf_GetActive()->spi1);
}

HAL_StatusTypeDef Trigger_SwapBuffer(void)
{
    TxBuf_Swap();
    return trigger_arm_and_run(TxBuf_GetActive()->spi1);
}

/* Trigger_GetCaptureTiming removed 2026-08-08: the TIM4 capture read-back
 * chain had no callers; re-add when TIM8 IO_UPDATE delay calibration is
 * implemented. */
