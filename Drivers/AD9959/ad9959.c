/**
 ******************************************************************************
 * @file    ad9959.c
 * @brief   AD9959 DDS Driver Implementation
 *
 * 2-wire serial mode:
 *   SDIO0 = SPI1_MOSI (PD7), SDIO1 = SPI3_MOSI (PD6)
 *   SCLK  = SPI1_SCK (PB3) + SPI3_SCK (PC10) synchronous
 *   CS    = SPI1_NSS (PA15) hardware pulse mode
 *
 * Phase 1: both SPI lanes send identical data (redundant dual-wire).
 * Encoder pre-splits data if bit-interleaving is needed later.
 ******************************************************************************
 */

#include "ad9959.h"
#include "ad9959_reg.h"
#include "bsp_spi.h"
#include "bsp_gpio.h"
#include "bsp_tim.h"
#include "bsp_dwt.h"
#include "hc595.h"
#include "phase1_config.h"
#include "debug_state.h"
#include <string.h>

AD9959_OneBitDebug ad9959_onebit_debug = {0};
AD9959_HardwareSpiDebug ad9959_hwspi_debug = {0};
extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi3;

typedef struct {
    volatile bool active, busy, update_pending;
    volatile uint32_t sent_count, error_count;
    uint32_t period_ms, next_tick;
} AD9959_CyclicTx;
static AD9959_CyclicTx ad9959_cyclic_tx = {0};
static uint8_t ad9959_cyclic_frame[32] __attribute__((section(".dma_buffer"), aligned(32)));

/* Retained only as reference for the GPIO bring-up experiment. The active
 * single-line test below uses SPI1 hardware exclusively. */
#if AD9959_DIRECT_CW_READBACK
static bool ad9959_onebit_bus_active = false;

static void AD9959_OneBitBusBegin(void)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

    gpio.Pin = SPI_9959_CS_Pin;
    HAL_GPIO_Init(SPI_9959_CS_GPIO_Port, &gpio);
    gpio.Pin = SPI_9959_SCLK_Pin;
    HAL_GPIO_Init(SPI_9959_SCLK_GPIO_Port, &gpio);
    gpio.Pin = SPI_9959_DIO0_Pin | SPI_9959_DIO1_Pin;
    HAL_GPIO_Init(SPI_9959_DIO0_GPIO_Port, &gpio);

    /* In 3-wire read mode AD9959 drives SDO on SDIO2 (PB4). */
    gpio.Pin = SPI_9959_IO2_R_Pin;
    gpio.Mode = GPIO_MODE_INPUT;
    HAL_GPIO_Init(SPI_9959_IO2_R_GPIO_Port, &gpio);

    HAL_GPIO_WritePin(SPI_9959_CS_GPIO_Port, SPI_9959_CS_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(SPI_9959_SCLK_GPIO_Port, SPI_9959_SCLK_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SPI_9959_DIO0_GPIO_Port, SPI_9959_DIO1_Pin, GPIO_PIN_RESET);
    ad9959_onebit_bus_active = true;
}

static void AD9959_OneBitBusEnd(void)
{
    GPIO_InitTypeDef gpio = {0};

    HAL_GPIO_WritePin(SPI_9959_CS_GPIO_Port, SPI_9959_CS_Pin, GPIO_PIN_SET);
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF5_SPI1;
    gpio.Pin       = SPI_9959_CS_Pin;
    HAL_GPIO_Init(SPI_9959_CS_GPIO_Port, &gpio);
    gpio.Pin = SPI_9959_SCLK_Pin;
    HAL_GPIO_Init(SPI_9959_SCLK_GPIO_Port, &gpio);
    gpio.Pin = SPI_9959_DIO0_Pin;
    HAL_GPIO_Init(SPI_9959_DIO0_GPIO_Port, &gpio);
    gpio.Pin = SPI_9959_IO2_R_Pin;
    gpio.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(SPI_9959_IO2_R_GPIO_Port, &gpio);
    gpio.Alternate = GPIO_AF5_SPI3;
    gpio.Pin       = SPI_9959_DIO1_Pin;
    HAL_GPIO_Init(SPI_9959_DIO1_GPIO_Port, &gpio);
    ad9959_onebit_bus_active = false;
}

/* PD5 and PB4 share the SDIO2 readback path. PD5 is normally held low for
 * the single-bit test, so release it before AD9959 drives SDIO2 for reads. */
static void AD9959_SDIO2_ReadbackEnable(bool enable)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = IO_9959_DIO2_Pin;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Mode = enable ? GPIO_MODE_INPUT : GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(IO_9959_DIO2_GPIO_Port, &gpio);

    if (!enable) {
        HAL_GPIO_WritePin(IO_9959_DIO2_GPIO_Port, IO_9959_DIO2_Pin,
                          GPIO_PIN_RESET);
    }
}

/* Read a complete register in AD9959 one-bit 3-wire mode. The instruction
 * is written on DIO0 and the returned SDO bits are sampled on SDIO2/PB4. */
static void AD9959_ReadRegister1Bit(uint8_t reg, uint8_t *data,
                                    uint8_t num_bytes)
{
    uint8_t instruction = (uint8_t)(reg | 0x80U);

    if (!ad9959_onebit_bus_active) {
        AD9959_OneBitBusBegin();
    }

    HAL_GPIO_WritePin(SPI_9959_CS_GPIO_Port, SPI_9959_CS_Pin, GPIO_PIN_SET);
    /* ADI single-bit read timing starts with SCLK high. */
    HAL_GPIO_WritePin(SPI_9959_SCLK_GPIO_Port, SPI_9959_SCLK_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(SPI_9959_DIO0_GPIO_Port, SPI_9959_DIO1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SPI_9959_CS_GPIO_Port, SPI_9959_CS_Pin, GPIO_PIN_RESET);

    for (uint8_t bit = 0x80U; bit != 0U; bit >>= 1U) {
        HAL_GPIO_WritePin(SPI_9959_SCLK_GPIO_Port, SPI_9959_SCLK_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(SPI_9959_DIO0_GPIO_Port, SPI_9959_DIO0_Pin,
                          (instruction & bit) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        HAL_GPIO_WritePin(SPI_9959_SCLK_GPIO_Port, SPI_9959_SCLK_Pin, GPIO_PIN_SET);
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
    }

    for (uint8_t byte_index = 0U; byte_index < num_bytes; byte_index++) {
        uint8_t value = 0U;
        for (uint8_t bit = 0x80U; bit != 0U; bit >>= 1U) {
            HAL_GPIO_WritePin(SPI_9959_SCLK_GPIO_Port, SPI_9959_SCLK_Pin, GPIO_PIN_RESET);
            __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
            if (HAL_GPIO_ReadPin(SPI_9959_IO2_R_GPIO_Port, SPI_9959_IO2_R_Pin) == GPIO_PIN_SET) {
                value |= bit;
            }
            HAL_GPIO_WritePin(SPI_9959_SCLK_GPIO_Port, SPI_9959_SCLK_Pin, GPIO_PIN_SET);
            __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        }
        data[byte_index] = value;
    }

    HAL_GPIO_WritePin(SPI_9959_CS_GPIO_Port, SPI_9959_CS_Pin, GPIO_PIN_SET);
}

static void AD9959_UpdateOneBitDebug(void)
{
    uint8_t fr1[3];
    uint8_t cftw[4];
    uint8_t acr[3];

    const uint8_t csr_3wire = CSR_CH0_ENABLE | CSR_IO_MODE_3WIRE;
    const uint8_t csr_2wire = CSR_CH0_ENABLE | CSR_IO_MODE_2WIRE;

    ad9959_onebit_debug.marker = 0U;
    AD9959_SDIO2_ReadbackEnable(true);
    /* CSR takes effect immediately. Select 3-wire mode so SDIO2 becomes
     * the serial-data output used for the following read transactions. */
    AD9959_WriteRegister(AD9959_REG_CSR, &csr_3wire, 1U);
    AD9959_ReadRegister1Bit(AD9959_REG_CSR, (uint8_t *)&ad9959_onebit_debug.csr, 1U);
    AD9959_ReadRegister1Bit(AD9959_REG_FR1, fr1, sizeof(fr1));
    AD9959_ReadRegister1Bit(AD9959_REG_CFR, (uint8_t *)ad9959_onebit_debug.cfr,
                            sizeof(ad9959_onebit_debug.cfr));
    AD9959_ReadRegister1Bit(AD9959_REG_CFTW, cftw, sizeof(cftw));
    AD9959_ReadRegister1Bit(AD9959_REG_ACR, acr, sizeof(acr));

    ad9959_onebit_debug.fr1 = ((uint32_t)fr1[0] << 16) |
                                ((uint32_t)fr1[1] << 8) | fr1[2];
    ad9959_onebit_debug.cftw = ((uint32_t)cftw[0] << 24) |
                                 ((uint32_t)cftw[1] << 16) |
                                 ((uint32_t)cftw[2] << 8) | cftw[3];
    ad9959_onebit_debug.acr = ((uint32_t)acr[0] << 16) |
                                ((uint32_t)acr[1] << 8) | acr[2];
    AD9959_OneBitBusEnd();
    /* Restore the direct-CW transport mode. This does not change any
     * channel register and does not require I/O_UPDATE. */
    AD9959_WriteRegister(AD9959_REG_CSR, &csr_2wire, 1U);
    AD9959_SDIO2_ReadbackEnable(false);
    ad9959_onebit_debug.marker = 0x99591B17UL;
}
#endif

/* ================================================================
 * Internal: Power-Up Sequence (via 595)
 * ================================================================ */

static void AD9959_PowerUpSequence(void)
{
    HC595_Write(0x0000);
    HC595_OutputEnable(false);
    HC595_ClearAll();
    DWT_Delay(2);

    hc595_dds_control.enable_1v8_digital = true;
    hc595_dds_control.enable_1v8_analog  = true;
    hc595_dds_control.enable_3v3_digital = true;
    hc595_dds_control.clk_mode_3v3       = true;
    /* Keep the DDS in master reset while all three supplies rise.  Releasing
     * reset during a regulator ramp made the PLL start state non-repeatable. */
    hc595_dds_control.master_reset       = true;
    /* PWR_DWN_CTL remains inactive throughout initialization.  MASTER_RESET
     * alone provides the required deterministic AD9959 reset. */
    hc595_dds_control.power_down         = false;
    HC595_ApplyDDSControl();

    /* Hold reset while the three DDS supplies settle. */
    HC595_OutputEnable(true);
    DWT_Delay(0.020);
}

/* ================================================================
 * Internal: PLL Configuration (FR1)
 * REF_CLK = 24.545 MHz (TIM15), PLL x20 = 490.909 MHz SYSCLK
 * ================================================================ */

static void AD9959_ConfigPLL(void)
{
    uint8_t fr1_data[3];
    uint32_t fr1_val = 0;

    /* VCO gain = HIGH (SYSCLK > 255 MHz)  [23] */
    fr1_val |= FR1_VCO_GAIN_HIGH;
    /* PLL divider = 20  [22:18] */
    fr1_val |= FR1_PLL_DIV(20);
    /* Match the validated 25 MHz reference design's loop-filter setting:
     * charge pump = 75 uA [17:16] = 00. */
    fr1_val |= FR1_CP_75uA;

    fr1_data[0] = (fr1_val >> 16) & 0xFF;
    fr1_data[1] = (fr1_val >> 8)  & 0xFF;
    fr1_data[2] =  fr1_val        & 0xFF;

    AD9959_WriteRegister(AD9959_REG_FR1, fr1_data, 3);
}

static void AD9959_ConfigureDirectIdlePins(void)
{
    GPIO_InitTypeDef gpio = {0};

    /* The direct-CW test uses SPI1 only.  Drive every unused DDS output low
     * and bias every passive DDS monitor input, so no AD9959 pin is floating. */
    HAL_GPIO_WritePin(GPIOD, IO_9959_3_Pin | IO_9959_2_Pin | IO_9959_1_Pin |
                      IO_9959_0_Pin | IO_9959_DIO2_Pin | SPI_9959_DIO1_Pin,
                      GPIO_PIN_RESET);
    gpio.Pin = IO_9959_3_Pin | IO_9959_2_Pin | IO_9959_1_Pin |
               IO_9959_0_Pin | IO_9959_DIO2_Pin | SPI_9959_DIO1_Pin;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOD, &gpio);

    HAL_GPIO_WritePin(GPIOC, SPI_9959_DIO3_Pin | GPIO_PIN_10, GPIO_PIN_RESET);
    gpio.Pin = SPI_9959_DIO3_Pin | GPIO_PIN_10;
    HAL_GPIO_Init(GPIOC, &gpio);

    HAL_GPIO_WritePin(SPI_9959_IO2_R_GPIO_Port, SPI_9959_IO2_R_Pin,
                      GPIO_PIN_RESET);
    gpio.Pin = SPI_9959_IO2_R_Pin;
    HAL_GPIO_Init(SPI_9959_IO2_R_GPIO_Port, &gpio);

    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Pin = SPI_9959_IO2_R_Pin | SYNC_9959_IO_UPDATE_BP_Pin;
    gpio.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOB, &gpio);
    gpio.Pin = SPI_9959_DIO3_BP_Pin;
    HAL_GPIO_Init(SPI_9959_DIO3_BP_GPIO_Port, &gpio);

    gpio.Pin = SPI_9959_CS_CAPTURE1_Pin | SPI_9959_CS_CAPTURE2_Pin;
    gpio.Pull = GPIO_PULLUP;  /* CS is inactive high. */
    HAL_GPIO_Init(GPIOB, &gpio);
}

void AD9959_IOUpdateGpioInit(void)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = SYNC_9959_IO_UPDATE_Pin;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(SYNC_9959_IO_UPDATE_GPIO_Port, &gpio);
    HAL_GPIO_WritePin(SYNC_9959_IO_UPDATE_GPIO_Port,
                      SYNC_9959_IO_UPDATE_Pin, GPIO_PIN_RESET);
}

/* The single-line setup drives IO_UPDATE as GPIO.  Before the runtime chain
 * starts, hand PC6 back to TIM8_CH1 exactly as CubeMX configured it. */
void AD9959_IOUpdateTimerInit(void)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = SYNC_9959_IO_UPDATE_Pin;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF3_TIM8;
    HAL_GPIO_Init(SYNC_9959_IO_UPDATE_GPIO_Port, &gpio);
}

static void AD9959_OfficialIOUpdate(void)
{
    HAL_GPIO_WritePin(SYNC_9959_IO_UPDATE_GPIO_Port,
                      SYNC_9959_IO_UPDATE_Pin, GPIO_PIN_RESET);
    //DWT_Delay(4.0e-6f);   /* Official capture: CS rising to IO_UPDATE rising ≈3.8 us. */
    HAL_GPIO_WritePin(SYNC_9959_IO_UPDATE_GPIO_Port,
                      SYNC_9959_IO_UPDATE_Pin, GPIO_PIN_SET);
    //DWT_Delay(6.4e-6f);
    HAL_GPIO_WritePin(SYNC_9959_IO_UPDATE_GPIO_Port,
                      SYNC_9959_IO_UPDATE_Pin, GPIO_PIN_RESET);
}

#if AD9959_SOFTWARE_SPI_TEST
/* Isolated GPIO transport for physical-layer diagnosis.  It deliberately
 * leaves the power, reset, REF_CLK and IO_UPDATE paths unchanged. */
static void AD9959_SoftwareSpiInit(void)
{
    GPIO_InitTypeDef gpio = {0};

    HAL_GPIO_WritePin(SPI_9959_CS_GPIO_Port, SPI_9959_CS_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(SPI_9959_SCLK_GPIO_Port, SPI_9959_SCLK_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SPI_9959_DIO0_GPIO_Port, SPI_9959_DIO0_Pin, GPIO_PIN_RESET);

    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Pin = SPI_9959_CS_Pin;
    HAL_GPIO_Init(SPI_9959_CS_GPIO_Port, &gpio);
    gpio.Pin = SPI_9959_SCLK_Pin;
    HAL_GPIO_Init(SPI_9959_SCLK_GPIO_Port, &gpio);
    gpio.Pin = SPI_9959_DIO0_Pin;
    HAL_GPIO_Init(SPI_9959_DIO0_GPIO_Port, &gpio);
}

static void AD9959_SoftwareSpiWriteByte(uint8_t data)
{
    for (uint8_t bit = 0x80U; bit != 0U; bit >>= 1U) {
        HAL_GPIO_WritePin(SPI_9959_DIO0_GPIO_Port, SPI_9959_DIO0_Pin,
                          (data & bit) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        DWT_Delay(1.0e-6f);
        HAL_GPIO_WritePin(SPI_9959_SCLK_GPIO_Port, SPI_9959_SCLK_Pin,
                          GPIO_PIN_SET);
        DWT_Delay(1.0e-6f);
        HAL_GPIO_WritePin(SPI_9959_SCLK_GPIO_Port, SPI_9959_SCLK_Pin,
                          GPIO_PIN_RESET);
    }
}

static HAL_StatusTypeDef AD9959_SoftwareSpiTransmit(const uint8_t *data,
                                                     uint8_t size)
{
    HAL_GPIO_WritePin(SPI_9959_CS_GPIO_Port, SPI_9959_CS_Pin, GPIO_PIN_RESET);
    DWT_Delay(1.0e-6f);
    for (uint8_t index = 0U; index < size; index++) {
        AD9959_SoftwareSpiWriteByte(data[index]);
    }
    DWT_Delay(1.0e-6f);
    HAL_GPIO_WritePin(SPI_9959_CS_GPIO_Port, SPI_9959_CS_Pin, GPIO_PIN_SET);
    DWT_Delay(1.0e-6f);
    return HAL_OK;
}
#endif

/* ================================================================
 * Public API
 * ================================================================ */

void AD9959_Init(void)
{
    ad9959_diag.init_stage = 0U;
    AD9959_ConfigureDirectIdlePins();
#if AD9959_SOFTWARE_SPI_TEST
    AD9959_SoftwareSpiInit();
#endif

    ad9959_hwspi_debug = (AD9959_HardwareSpiDebug){0};
    ad9959_hwspi_debug.software_spi = AD9959_SOFTWARE_SPI_TEST;

    /* Power-up: 1.8VD → 1.8VA → 3.3VD, master_reset held high throughout. */
    AD9959_PowerUpSequence();
    ad9959_diag.init_stage = 1U;

    /* Start REF_CLK, then release master_reset — aligned with Template1
     * IntReset() behaviour. */
    BSP_TIM15_SetREFCLK(25000000UL);
    BSP_TIM15_Start();
    HAL_Delay(1);
    ad9959_diag.init_stage = 2U;

    hc595_dds_control.master_reset = false;
    HC595_ApplyDDSControl();
    HAL_Delay(1);
    ad9959_diag.init_stage = 3U;

    /* FR1 (PLL) then FR2 (default) — exactly as Template1 init sequence. */
    AD9959_ConfigPLL();
    ad9959_diag.init_stage = 4U;
    const uint8_t fr2[2] = { 0x00U, 0x00U };
    AD9959_WriteRegister(AD9959_REG_FR2, fr2, sizeof(fr2));
    ad9959_diag.init_stage = 5U;
}

static void AD9959_SetCWMaskDirect(uint8_t csr, uint32_t ftw, uint16_t asf)
{
    const uint8_t cfr[3] = { 0x00U, 0x03U, 0x02U };
    uint8_t cftw[4] = {
        (uint8_t)(ftw >> 24), (uint8_t)(ftw >> 16),
        (uint8_t)(ftw >> 8), (uint8_t)ftw
    };
    uint16_t amplitude = (uint16_t)(ACR_AMP_MULT_ENABLE | (asf & ACR_ASF_Msk));
    uint8_t acr[3] = { 0x00U, (uint8_t)(amplitude >> 8), (uint8_t)amplitude };
    const uint8_t cpow[2] = { 0x00U, 0x00U };

    AD9959_WriteRegister(AD9959_REG_CSR, &csr, 1);
    AD9959_WriteRegister(AD9959_REG_CFR, cfr, sizeof(cfr));
    AD9959_WriteRegister(AD9959_REG_CFTW, cftw, sizeof(cftw));
    AD9959_WriteRegister(AD9959_REG_ACR, acr, sizeof(acr));
    AD9959_WriteRegister(AD9959_REG_CPOW, cpow, sizeof(cpow));
    AD9959_IOUpdate();
#if AD9959_DIRECT_CW_READBACK
    HAL_Delay(1);
    AD9959_UpdateOneBitDebug();
#endif
    ad9959_hwspi_debug.marker = 0x9959A501UL;
}

void AD9959_IOUpdate(void)
{
    HAL_GPIO_WritePin(SYNC_9959_IO_UPDATE_GPIO_Port,
                      SYNC_9959_IO_UPDATE_Pin, GPIO_PIN_RESET);
    DWT_Delay(1.0e-6f);
    HAL_GPIO_WritePin(SYNC_9959_IO_UPDATE_GPIO_Port,
                      SYNC_9959_IO_UPDATE_Pin, GPIO_PIN_SET);
    DWT_Delay(1.0e-6f);
    HAL_GPIO_WritePin(SYNC_9959_IO_UPDATE_GPIO_Port,
                      SYNC_9959_IO_UPDATE_Pin, GPIO_PIN_RESET);
}

void AD9959_DebugSpiWaveformTest(void)
{
    const uint8_t aa = 0xAAU;
    const uint8_t bit55 = 0x55U;

    AD9959_IOUpdateGpioInit();
    hc595_dds_control.master_reset = true;
    HC595_ApplyDDSControl();

    while (1) {
        AD9959_WriteRegister(AD9959_REG_CSR, &aa, 1U);
        AD9959_WriteRegister(AD9959_REG_CSR, &bit55, 1U);
    }
}

void AD9959_Reset(void)
{
    hc595_dds_control.master_reset = true;
    HC595_ApplyDDSControl();
    HAL_Delay(1);
    hc595_dds_control.master_reset = false;
    HC595_ApplyDDSControl();
}

void AD9959_PowerDown(bool enable)
{
    hc595_dds_control.power_down = enable;
    HC595_ApplyDDSControl();
}

/* ================================================================
 * Register Access
 * ================================================================ */

void AD9959_WriteRegister(uint8_t reg, const uint8_t *data, uint8_t num_bytes)
{
    uint8_t frame[32];
    if (num_bytes > 31) return;
    frame[0] = reg & 0x7F;
    memcpy(frame + 1, data, num_bytes);
    uint8_t total = num_bytes + 1;

    /* The transport is selected only for the isolated direct-CW test. */
#if AD9959_SOFTWARE_SPI_TEST
    HAL_StatusTypeDef status = AD9959_SoftwareSpiTransmit(frame, total);
#else
    HAL_StatusTypeDef status = BSP_SPI_TransmitStatus(SPI1, frame, total);
#endif
    uint32_t status_value = (uint32_t)status;

    switch (reg & 0x7FU) {
    case AD9959_REG_FR1:  ad9959_hwspi_debug.fr1_status  = status_value; break;
    case AD9959_REG_CSR:  ad9959_hwspi_debug.csr_status  = status_value; break;
    case AD9959_REG_CFR:  ad9959_hwspi_debug.cfr_status  = status_value; break;
    case AD9959_REG_CFTW: ad9959_hwspi_debug.cftw_status = status_value; break;
    case AD9959_REG_ACR:  ad9959_hwspi_debug.acr_status  = status_value; break;
    case AD9959_REG_CPOW: ad9959_hwspi_debug.cpow_status = status_value; break;
    default: break;
    }
    ad9959_hwspi_debug.spi1_error = BSP_SPI_GetError(SPI1);
    ad9959_hwspi_debug.transaction_count++;
}

void AD9959_ReadRegister(uint8_t reg, uint8_t *data, uint8_t num_bytes)
{
#if AD9959_DIRECT_CW_READBACK
    AD9959_ReadRegister1Bit(reg, data, num_bytes);
    AD9959_OneBitBusEnd();
#else
    (void)reg;
    memset(data, 0, num_bytes);
#endif
}

void AD9959_SetCWDirect(uint8_t channel, uint32_t ftw, uint16_t asf)
{
    AD9959_SetCWMaskDirect(CSR_CHANNEL(channel), ftw, asf);
}

void AD9959_SetCWAllDirect(uint32_t ftw, uint16_t asf)
{
    AD9959_SetCWMaskDirect(CSR_CHANNEL_MASK, ftw, asf);
}

void AD9959_SetCWOfficialPerChannel(uint32_t ftw, uint16_t asf)
{
    const uint8_t cftw[4] = {
        (uint8_t)(ftw >> 24), (uint8_t)(ftw >> 16),
        (uint8_t)(ftw >> 8), (uint8_t)ftw
    };
    const uint16_t phase[4] = { 0x0000U, 0x0FFFU, 0x2FFDU, 0x1FFEU };
    (void)asf;

    /* Match the official order: select one channel, then write its CFTW. */
    for (uint8_t channel = 0U; channel < 4U; channel++) {
        const uint8_t csr = CSR_CHANNEL(channel);
        AD9959_WriteRegister(AD9959_REG_CSR, &csr, 1U);
        AD9959_WriteRegister(AD9959_REG_CFTW, cftw, sizeof(cftw));
    }
    /* Then write four individual phase words before one common update. */
    for (uint8_t channel = 0U; channel < 4U; channel++) {
        const uint8_t csr = CSR_CHANNEL(channel);
        const uint8_t cpow[2] = { (uint8_t)(phase[channel] >> 8),
                                  (uint8_t)phase[channel] };
        AD9959_WriteRegister(AD9959_REG_CSR, &csr, 1U);
        AD9959_WriteRegister(AD9959_REG_CPOW, cpow, sizeof(cpow));
    }
    AD9959_OfficialIOUpdate();
}

void AD9959_SetCWOfficialChannel(uint8_t channel, uint32_t ftw, uint16_t asf)
{
    const uint8_t csr = CSR_CHANNEL(channel);
    const uint8_t cftw[4] = {
        (uint8_t)(ftw >> 24), (uint8_t)(ftw >> 16),
        (uint8_t)(ftw >> 8), (uint8_t)ftw
    };
    const uint16_t phase[4] = { 0x0000U, 0x0FFFU, 0x2FFDU, 0x1FFEU };
    const uint8_t cpow[2] = { (uint8_t)(phase[channel] >> 8),
                              (uint8_t)phase[channel] };
    (void)asf;

    /* Validated official CH1 test sequence: CFTW and CPOW only, with each
     * register in an independent CS window. */
    AD9959_WriteRegister(AD9959_REG_CSR, &csr, 1U);
    AD9959_WriteRegister(AD9959_REG_CFTW, cftw, sizeof(cftw));
    AD9959_WriteRegister(AD9959_REG_CSR, &csr, 1U);
    AD9959_WriteRegister(AD9959_REG_CPOW, cpow, sizeof(cpow));
    AD9959_OfficialIOUpdate();
    ad9959_diag.init_stage = 7U;
}

bool AD9959_Enable2BitSerial(uint8_t channel)
{
    if (channel >= 4U) {
        return false;
    }

    /* AD9959 starts in one-bit mode. CSR takes effect immediately, so this
     * transaction must remain 8-bit SPI1/DIO0 before both SPI peripherals are
     * converted to the encoder's 4-bit, two-lane DMA representation. */
    const uint8_t csr_2bit = CSR_CHANNEL(channel) | CSR_IO_MODE_2BIT;
    AD9959_WriteRegister(AD9959_REG_CSR, &csr_2bit, 1U);

    if (HAL_SPI_DeInit(&hspi1) != HAL_OK || HAL_SPI_DeInit(&hspi3) != HAL_OK) {
        return false;
    }
    hspi1.Init.DataSize = SPI_DATASIZE_4BIT;
    hspi3.Init.DataSize = SPI_DATASIZE_4BIT;
    if (HAL_SPI_Init(&hspi1) != HAL_OK || HAL_SPI_Init(&hspi3) != HAL_OK) {
        return false;
    }
    AD9959_IOUpdateTimerInit();
    ad9959_diag.init_stage = 8U;
    return true;
}

void AD9959_CyclicFTW_Start(uint32_t ftw, uint32_t period_ms)
{
    GPIO_InitTypeDef gpio = {0};
    HAL_DMA_MuxSyncConfigTypeDef mux = {0};

    /* DMA updates use GPIO CS; SPI1 remains the hardware SCLK/DIO0 source. */
    HAL_GPIO_WritePin(SPI_9959_CS_GPIO_Port, SPI_9959_CS_Pin, GPIO_PIN_SET);
    gpio.Pin = SPI_9959_CS_Pin;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(SPI_9959_CS_GPIO_Port, &gpio);

    /* Direct-CW mode does not run LPTIM1, so remove the buffered DMA gate. */
    mux.SyncSignalID = HAL_DMAMUX1_SYNC_LPTIM1_OUT;
    mux.SyncPolarity = HAL_DMAMUX_SYNC_NO_EVENT;
    mux.SyncEnable = DISABLE;
    mux.EventEnable = DISABLE;
    mux.RequestNumber = 1U;
    (void)HAL_DMAEx_ConfigMuxSync(hspi1.hdmatx, &mux);

    ad9959_cyclic_frame[0] = AD9959_REG_CFTW;
    ad9959_cyclic_frame[1] = (uint8_t)(ftw >> 24);
    ad9959_cyclic_frame[2] = (uint8_t)(ftw >> 16);
    ad9959_cyclic_frame[3] = (uint8_t)(ftw >> 8);
    ad9959_cyclic_frame[4] = (uint8_t)ftw;
    ad9959_cyclic_tx.period_ms = period_ms ? period_ms : 1U;
    ad9959_cyclic_tx.next_tick = HAL_GetTick();
    ad9959_cyclic_tx.busy = false;
    ad9959_cyclic_tx.update_pending = false;
    ad9959_cyclic_tx.sent_count = 0U;
    ad9959_cyclic_tx.error_count = 0U;
    ad9959_cyclic_tx.active = true;
}

void AD9959_CyclicFTW_Task(void)
{
    if (!ad9959_cyclic_tx.active) return;
    if (ad9959_cyclic_tx.update_pending) {
        ad9959_cyclic_tx.update_pending = false;
        AD9959_IOUpdate();
        ad9959_cyclic_tx.next_tick = HAL_GetTick() + ad9959_cyclic_tx.period_ms;
        return;
    }
    if (ad9959_cyclic_tx.busy ||
        (int32_t)(HAL_GetTick() - ad9959_cyclic_tx.next_tick) < 0) return;

    SCB_CleanDCache_by_Addr((uint32_t *)ad9959_cyclic_frame, 32U);
    HAL_GPIO_WritePin(SPI_9959_CS_GPIO_Port, SPI_9959_CS_Pin, GPIO_PIN_RESET);
    if (HAL_SPI_Transmit_DMA(&hspi1, ad9959_cyclic_frame, 5U) == HAL_OK) {
        ad9959_cyclic_tx.busy = true;
    } else {
        HAL_GPIO_WritePin(SPI_9959_CS_GPIO_Port, SPI_9959_CS_Pin, GPIO_PIN_SET);
        ad9959_cyclic_tx.error_count++;
        ad9959_cyclic_tx.next_tick = HAL_GetTick() + ad9959_cyclic_tx.period_ms;
    }
}

void AD9959_CyclicFTW_OnSpiTxComplete(void)
{
    if (!ad9959_cyclic_tx.active || !ad9959_cyclic_tx.busy) return;
    HAL_GPIO_WritePin(SPI_9959_CS_GPIO_Port, SPI_9959_CS_Pin, GPIO_PIN_SET);
    ad9959_cyclic_tx.busy = false;
    ad9959_cyclic_tx.update_pending = true;
    ad9959_cyclic_tx.sent_count++;
}

void AD9959_CyclicFTW_OnSpiError(void)
{
    if (!ad9959_cyclic_tx.active) return;
    HAL_GPIO_WritePin(SPI_9959_CS_GPIO_Port, SPI_9959_CS_Pin, GPIO_PIN_SET);
    ad9959_cyclic_tx.busy = false;
    ad9959_cyclic_tx.error_count++;
    ad9959_cyclic_tx.next_tick = HAL_GetTick() + ad9959_cyclic_tx.period_ms;
}

/* ================================================================
 * High-Level Setters
 * ================================================================ */

void AD9959_SetFTW(uint8_t channel, uint32_t ftw)
{
    /* CSR[7:4] selects target channel before CFTW write */
    uint8_t csr = CSR_CHANNEL(channel);
    AD9959_WriteRegister(AD9959_REG_CSR, &csr, 1);

    uint8_t buf[4];
    buf[0] = (ftw >> 24) & 0xFF;
    buf[1] = (ftw >> 16) & 0xFF;
    buf[2] = (ftw >> 8)  & 0xFF;
    buf[3] =  ftw        & 0xFF;
    AD9959_WriteRegister(AD9959_REG_CFTW, buf, 4);
}

void AD9959_SetASF(uint8_t channel, uint16_t asf)
{
    /* CSR[7:4] selects channel; ACR has NO internal channel bits */
    uint8_t csr = CSR_CHANNEL(channel);
    AD9959_WriteRegister(AD9959_REG_CSR, &csr, 1);

    uint8_t buf[3];
    uint32_t acr = ((uint32_t)(asf & ACR_ASF_Msk) << ACR_ASF_Pos) |
                   ACR_AMP_MULT_ENABLE;
    buf[0] = (acr >> 16) & 0xFF;
    buf[1] = (acr >> 8)  & 0xFF;
    buf[2] =  acr        & 0xFF;
    AD9959_WriteRegister(AD9959_REG_ACR, buf, 3);
}

void AD9959_SetPOW(uint8_t channel, uint16_t pow)
{
    uint8_t csr = CSR_CHANNEL(channel);
    AD9959_WriteRegister(AD9959_REG_CSR, &csr, 1);

    uint8_t buf[2];
    buf[0] = (pow >> 8) & 0xFF;
    buf[1] =  pow       & 0xFF;
    AD9959_WriteRegister(AD9959_REG_CPOW, buf, 2);
}

void AD9959_SetChannelMask(uint8_t channel_mask)
{
    /* channel_mask uses CSR[7:4]. Also set 2-wire mode [2:1]=00, MSB first [0]=0. */
    uint8_t csr = (channel_mask << 4) & CSR_CHANNEL_MASK;
    csr |= CSR_IO_MODE_2BIT;
    AD9959_WriteRegister(AD9959_REG_CSR, &csr, 1);
}
