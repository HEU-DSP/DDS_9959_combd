/**
 ******************************************************************************
 * @file    hc595.c
 * @brief   74HC595 Driver Implementation
 *
 * Transport: GPIO bit-bang — PE3 = DS, PE2 = STCP, PE4 = SHCP,
 * PE5 = MR (active-low), PE6 = OE (active-low).  (An earlier USART10
 * half-duplex transport was planned but never implemented.)
 *
 * Each 16-bit write shifts chip2_data[15:8] first, then chip1_data[7:0];
 * the data shifts into chip1 and chip1's QH' overflows into chip2's DS.
 *
 * After writing, HC595_Latch() toggles STCP to push shift register
 * contents to parallel outputs.
 *
 * CONCURRENCY: HC595_Write() is called both from the main loop (power-up
 * sequence, APPLY, reset — DDS control bits) and from the TIM5 10 Hz ISR
 * (Led_Refresh → HC595_WriteLEDs).  It therefore runs under a PRIMASK
 * critical section so an ISR cannot tear a half-shifted frame: bit 11 of
 * chip2 is AD9959 master_reset, and a torn frame could pulse it and wipe
 * the boot-once FR1/PLL configuration.
 ******************************************************************************
 */

#include "hc595.h"
#include "bsp_gpio.h"

/* Shadow registers for read-modify-write */
static uint16_t hc595_shadow = 0x0000;
HC595_DDSControl hc595_dds_control = {0};

static uint16_t HC595_PackDDSControl(uint16_t value)
{
    /* U20 Q0 is not connected. The DDS controls are U20 Q1..Q6,
     * which occupy raw shift-register bits 9..14 in the 16-bit frame. */
    const uint16_t dds_mask = 0x7E00U;
    uint16_t dds_bits = 0U;

    if (hc595_dds_control.enable_1v8_digital)  dds_bits |= (1U << 9);
    if (hc595_dds_control.enable_1v8_analog)   dds_bits |= (1U << 10);
    if (hc595_dds_control.master_reset)        dds_bits |= (1U << 11);
    if (hc595_dds_control.power_down)          dds_bits |= (1U << 12);
    if (hc595_dds_control.enable_3v3_digital)  dds_bits |= (1U << 13);
    if (hc595_dds_control.clk_mode_3v3)        dds_bits |= (1U << 14);

    return (value & ~dds_mask) | dds_bits;
}

/* ================================================================
 * Initialization
 * ================================================================ */

void HC595_Init(void)
{
    /* GPIOs are configured by CubeMX as push-pull outputs. */
    BSP_GPIO_595_MR_Set();
    BSP_GPIO_595_OE_Set();
    BSP_GPIO_595_STCP_Clr();
    BSP_GPIO_595_SHCP_Clr();
    HAL_GPIO_WritePin(OCR_DS_GPIO_Port, OCR_DS_Pin, GPIO_PIN_RESET);
    HC595_Reset();
    hc595_shadow = 0x0000;
    hc595_dds_control = (HC595_DDSControl){0};
}

/* ================================================================
 * Write Operations
 * ================================================================ */

void HC595_Write(uint16_t data)
{
    /* Bit-bang 16 bits MSB-first via GPIO:
     * Standard cascade: first byte → chip1, second byte → chip2.
     * Each byte: MSB first (standard SPI-like shift). */

    /* The whole frame must be indivisible w.r.t. the TIM5 ISR
     * (Led_Refresh → HC595_WriteLEDs).  PRIMASK save/restore — NOT an
     * unconditional __enable_irq() — so a call made from inside an
     * existing critical section (or from the ISR itself, where PRIMASK
     * is already set) stays correct and nesting works. */
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    uint16_t mask = 0x8000U;
    BSP_GPIO_595_STCP_Clr();
    for (int i = 0; i < 16; i++) {
        BSP_GPIO_595_SHCP_Clr();
        /* Set DS: bit 15 first (MSB of chip2 = last bit shifted into chip1) */
        if (data & mask)
            HAL_GPIO_WritePin(OCR_DS_GPIO_Port, OCR_DS_Pin, GPIO_PIN_SET);
        else
            HAL_GPIO_WritePin(OCR_DS_GPIO_Port, OCR_DS_Pin, GPIO_PIN_RESET);
        BSP_GPIO_595_SHCP_Set();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        BSP_GPIO_595_SHCP_Clr();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        mask >>= 1U;
    }

    HC595_Latch();
    hc595_shadow = data;

    if (primask == 0U) {
        __enable_irq();
    }
}

void HC595_SetBit(uint8_t chip, uint8_t bit)
{
    if (bit < 1 || bit > 7) return;
    if (chip == 1) {
        hc595_shadow |=  (1UL << bit);        /* bit = Q pin (1-7) */
    } else if (chip == 2) {
        hc595_shadow |=  (1UL << (bit + 8));  /* bit = Q pin → shadow[15:8] */
    }
    HC595_Write(hc595_shadow);
}

void HC595_ClrBit(uint8_t chip, uint8_t bit)
{
    if (bit < 1 || bit > 7) return;
    if (chip == 1) {
        hc595_shadow &= ~(1UL << bit);
    } else if (chip == 2) {
        hc595_shadow &= ~(1UL << (bit + 8));
    }
    HC595_Write(hc595_shadow);
}

void HC595_ClearAll(void)
{
    hc595_shadow = 0x0000;
    HC595_Write(0x0000);
}

void HC595_ApplyDDSControl(void)
{
    HC595_Write(HC595_PackDDSControl(hc595_shadow));
}

/* ================================================================
 * Control Signals
 * ================================================================ */

void HC595_Latch(void)
{
    BSP_GPIO_595_STCP_Set();
    __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
    BSP_GPIO_595_STCP_Clr();
}

void HC595_Reset(void)
{
    BSP_GPIO_595_MR_Clr();
    for (volatile int i = 0; i < 10; i++) {}
    BSP_GPIO_595_MR_Set();
}

void HC595_WriteLEDs(uint8_t leds)
{
    uint16_t led_bits = 0U;
    if (leds & 0x01U) led_bits |= (1U << HC595_CH1_BIT_LEDSTBY);
    if (leds & 0x02U) led_bits |= (1U << HC595_CH1_BIT_LED_ANALOGREADY);
    if (leds & 0x04U) led_bits |= (1U << HC595_CH1_BIT_LEDMODREADY);
    if (leds & 0x08U) led_bits |= (1U << HC595_CH1_BIT_LEDCH0TRANSMIT);
    if (leds & 0x10U) led_bits |= (1U << HC595_CH1_BIT_LEDCH1TRANSMIT);
    if (leds & 0x20U) led_bits |= (1U << HC595_CH1_BIT_LEDCH2TRANSMIT);
    if (leds & 0x40U) led_bits |= (1U << HC595_CH1_BIT_LEDCH3TRANSMIT);
    hc595_shadow = (hc595_shadow & 0xFF00U) | led_bits;
    HC595_Write(hc595_shadow);
}

void HC595_OutputEnable(bool enable)
{
    if (enable)
        BSP_GPIO_595_OE_Clr();   /* OE low = outputs enabled */
    else
        BSP_GPIO_595_OE_Set();   /* OE high = Hi-Z */
}
