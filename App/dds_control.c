/**
 ******************************************************************************
 * @file    dds_control.c
 * @brief   Staging config + APPLY safe-switch implementation.
 ******************************************************************************
 */

#include "dds_control.h"
#include "mod_config.h"
#include "ad9959.h"
#include "ad9959_reg.h"
#include "dds_encoder.h"
#include "bsp_tim.h"
#include "bsp_spi.h"
#include "channel_config.h"
#include "symbol_buffer.h"
#include "tx_buffer.h"
#include "phase1_config.h"
#if !AD9959_DOWNGRADE_MODE
#include "trigger.h"
#endif
#include <string.h>

/* ---- External handles ---- */
extern TIM_HandleTypeDef htim8;
extern SPI_HandleTypeDef hspi1;

/* ---- Forward declarations ---- */
static uint8_t dds_apply(void);
static uint8_t dds_stop_output(void);
static uint8_t dds_apply_config(void);

/* ---- Staging configuration ---- */
ChannelModConfig pending_cfg[DDS_CHANNEL_COUNT];
bool             pending_cfg_has_data;

/* ---- Internal state ---- */
static volatile bool apply_requested;
static volatile bool stop_requested;
static volatile bool busy;

/* ================================================================
 * Public API
 * ================================================================ */

void DDSControl_RequestApply(void)
{
    apply_requested = true;
}

void DDSControl_RequestStop(void)
{
    stop_requested = true;
}

bool DDSControl_IsBusy(void)
{
    return busy;
}

uint8_t DDSControl_Task(void)
{
    if (!apply_requested && !stop_requested) {
        return DDS_APPLY_OK;
    }
    if (busy) {
        return DDS_APPLY_ERR_BUSY;
    }
    busy = true;

    if (stop_requested) {
        stop_requested = false;
        uint8_t result = dds_stop_output();
        busy = false;
        return result;
    }

    if (apply_requested) {
        apply_requested = false;
        uint8_t result = dds_apply();
        busy = false;
        return result;
    }

    busy = false;
    return DDS_APPLY_OK;
}

/* ================================================================
 * Internal: shared config write (both modes)
 * ================================================================ */

/**
 * @brief  Copy pending_cfg into mod_cfg[] and write static registers.
 *
 * Runs with PC6 already borrowed as GPIO (IO_UPDATE via software).
 * Shared by downgrade and DMA APPLY paths.
 *
 * @return DDS_APPLY_OK on success.
 */
static uint8_t dds_apply_config(void)
{
    /* 1. Switch PC6 to GPIO for IO_UPDATE */
    AD9959_IOUpdateGpioInit();

    /* 2. Copy pending_cfg to live mod_cfg[] */
    uint8_t channel_mask = 0U;
    for (uint8_t ch = 0; ch < DDS_CHANNEL_COUNT; ch++) {
        if (pending_cfg[ch].enabled) {
            channel_mask |= (1U << ch);
        }
        /* Copy the full struct — all modulation params */
        memcpy(&mod_cfg[ch], &pending_cfg[ch], sizeof(ChannelModConfig));
    }
    pending_cfg_has_data = false;

    /* 3. Write CSR + CFR static regs for every enabled channel */
    Encoder_WriteStaticRegs(channel_mask);

    /* 4. Write initial CFTW for CW channels so output is immediate */
    for (uint8_t ch = 0; ch < DDS_CHANNEL_COUNT; ch++) {
        if (!mod_cfg[ch].enabled) continue;
        if (mod_cfg[ch].mode != CH_MODE_CW) continue;

        uint8_t csr = CSR_CHANNEL(ch);
        AD9959_WriteRegister(AD9959_REG_CSR, &csr, 1);
        uint8_t buf[4];
        uint32_t ftw = mod_cfg[ch].cw.ftw;
        buf[0] = (ftw >> 24) & 0xFF;
        buf[1] = (ftw >> 16) & 0xFF;
        buf[2] = (ftw >> 8)  & 0xFF;
        buf[3] =  ftw        & 0xFF;
        AD9959_WriteRegister(AD9959_REG_CFTW, buf, 4);

        csr = CSR_CHANNEL(ch);
        AD9959_WriteRegister(AD9959_REG_CSR, &csr, 1);
        uint32_t acr = ((uint32_t)(mod_cfg[ch].cw.asf & ACR_ASF_Msk)
                        << ACR_ASF_Pos) | ACR_AMP_MULT_ENABLE;
        buf[0] = (acr >> 16) & 0xFF;
        buf[1] = (acr >> 8)  & 0xFF;
        buf[2] =  acr        & 0xFF;
        AD9959_WriteRegister(AD9959_REG_ACR, buf, 3);
    }

    /* 5. GPIO IO_UPDATE pulse */
    AD9959_IOUpdate();

    /* 6. Restore PC6 to TIM8_CH1 AF */
    AD9959_IOUpdateTimerInit();

    return DDS_APPLY_OK;
}

/* ================================================================
 * Internal: APPLY safe-switch
 * ================================================================ */

#if AD9959_DOWNGRADE_MODE
static uint8_t dds_apply(void)
{
    /* 1. Save TIM8 state */
    uint32_t tim8_arr  = TIM8->ARR;
    uint32_t tim8_ccr1 = TIM8->CCR1;
    uint32_t tim8_dier = TIM8->DIER;

    /* 2. Disable TIM8 update interrupt */
    TIM8->DIER &= ~TIM_IT_UPDATE;

    /* 3. Wait for any in-flight ISR to complete + SPI1 idle */
    for (volatile int i = 0; i < 200; i++) { __NOP(); }
    if (HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY) {
        /* SPI1 busy — restore and return error */
        TIM8->DIER = tim8_dier;
        return DDS_APPLY_ERR_SPI;
    }

    /* 4-9. Copy config + static regs + GPIO IO_UPDATE */
    (void)dds_apply_config();

    /* 10. Rebuild pre_encoded[] for the TIM8 ISR */
    SymbolBuf_Clear();
    Encoder_BuildBank();

    /* 11. Restore TIM8 state and re-enable */
    TIM8->ARR  = tim8_arr;
    TIM8->CCR1 = tim8_ccr1;
    TIM8->DIER = tim8_dier;

    return DDS_APPLY_OK;
}
#else /* DMA mode */
static uint8_t dds_apply(void)
{
    /* 1. Stop the trigger chain (LPTIM3 + TIM8 + DMA abort) */
    Trigger_Stop();

    /* 2. Wait for any in-flight DMA/SPI activity to settle */
    for (volatile int i = 0; i < 200; i++) { __NOP(); }
    if (HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY) {
        /* SPI1 busy — resume the chain and report error */
        Trigger_Restart();
        return DDS_APPLY_ERR_SPI;
    }

    /* 3-8. Copy config + static regs + GPIO IO_UPDATE */
    (void)dds_apply_config();

    /* 9. Rebuild both ping-pong banks (zero-copy into D2 SRAM) */
    SymbolBuf_Clear();
    tx_bank_bytes = Encoder_BuildBank_DMA();
    TxBuf_Swap();
    tx_bank_bytes = Encoder_BuildBank_DMA();
    TxBuf_Swap();

    /* 10. Restart the trigger chain */
    Trigger_Restart();

    return DDS_APPLY_OK;
}
#endif

/* ================================================================
 * Internal: STOP_OUTPUT
 * ================================================================ */

#if AD9959_DOWNGRADE_MODE
static uint8_t dds_stop_output(void)
{
    uint32_t tim8_dier = TIM8->DIER;

    /* Disable TIM8 ISR */
    TIM8->DIER &= ~TIM_IT_UPDATE;

    for (volatile int i = 0; i < 200; i++) { __NOP(); }
    if (HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY) {
        TIM8->DIER = tim8_dier;
        return DDS_APPLY_ERR_SPI;
    }

    /* Disable all channels */
    for (uint8_t ch = 0; ch < DDS_CHANNEL_COUNT; ch++) {
        mod_cfg[ch].enabled = false;
        mod_cfg[ch].mode    = CH_MODE_OFF;
    }

    SymbolBuf_Clear();
    Encoder_BuildBank();

    TIM8->DIER = tim8_dier;
    return DDS_APPLY_OK;
}
#else /* DMA mode */
static uint8_t dds_stop_output(void)
{
    /* Stop the trigger chain — output ceases immediately. */
    Trigger_Stop();

    for (volatile int i = 0; i < 200; i++) { __NOP(); }
    if (HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY) {
        Trigger_Restart();
        return DDS_APPLY_ERR_SPI;
    }

    /* Disable all channels */
    for (uint8_t ch = 0; ch < DDS_CHANNEL_COUNT; ch++) {
        mod_cfg[ch].enabled = false;
        mod_cfg[ch].mode    = CH_MODE_OFF;
    }

    /* Empty banks — no DMA frame until the next APPLY. */
    SymbolBuf_Clear();
    tx_bank_bytes = Encoder_BuildBank_DMA();   /* 0 with no channels enabled */

    return DDS_APPLY_OK;
}
#endif
