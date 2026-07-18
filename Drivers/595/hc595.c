/**
 ******************************************************************************
 * @file    hc595.c
 * @brief   74HC595 Driver Implementation
 *
 * USART10 is configured in half-duplex single-wire mode (TX only).
 * Each 16-bit write sends: chip1_data[7:0] then chip2_data[15:8]
 * as a single USART frame (LSB first or MSB first depending on
 * USART configuration). The data shifts into chip1, and chip1's
 * QH' overflows into chip2's DS.
 *
 * After writing, HC595_Latch() toggles STCP to push shift register
 * contents to parallel outputs.
 ******************************************************************************
 */

#include "hc595.h"
#include "bsp_gpio.h"
#include "bsp_uart.h"

/* Shadow registers for read-modify-write */
static uint16_t hc595_shadow = 0x0000;

/* ================================================================
 * Initialization
 * ================================================================ */

void HC595_Init(void)
{
    /* GPIOs already initialized by CubeMX MX_GPIO_Init().
     * USART10 already initialized by CubeMX MX_USART10_UART_Init().
     * Set default state: MR high (not reset), OE high (outputs disabled) */
    BSP_GPIO_595_MR_Set();
    BSP_GPIO_595_OE_Set();
    BSP_GPIO_595_STCP_Clr();
    BSP_GPIO_595_SHCP_Clr();
    hc595_shadow = 0x0000;
}

/* ================================================================
 * Write Operations
 * ================================================================ */

void HC595_Write(uint16_t data)
{
    /* USART10 frame = 16-bit. Due to half-duplex mode,
     * TX pin toggles per bit at baud rate.
     * AD9959-side SHCP must be driven by a separate GPIO (PE4)
     * or derived from the USART clock.
     *
     * Current approach: use USART10 TX to send 16 bits as a
     * UART frame. The UART TX pin (PE3) drives DS directly.
     * SHCP (PE4) must be toggled by software in sync with
     * USART bit timing — OR use USART10's TX pin mode only
     * for DS, and manually clock SHCP.
     *
     * Simpler Phase 1 approach: bit-bang via GPIO.
     * (USART10 auto-clocking may require external wiring
     * of TX to SHCP, which is not the case here.) */

    /* Bit-bang 16 bits: MSB first (chip2→chip1 cascade order)
     * Standard cascade: first byte → chip1, second byte → chip2.
     * USART frame order: send chip1 byte first, then chip2 byte.
     * Each byte: MSB first (standard SPI-like shift). */

    uint16_t val = data;
    for (int i = 0; i < 16; i++) {
        BSP_GPIO_595_SHCP_Clr();
        /* Set DS: bit 15 first (MSB of chip2 = last bit shifted into chip1) */
        if (val & 0x8000)
            HAL_GPIO_WritePin(OCR_DS_GPIO_Port, OCR_DS_Pin, GPIO_PIN_SET);
        else
            HAL_GPIO_WritePin(OCR_DS_GPIO_Port, OCR_DS_Pin, GPIO_PIN_RESET);
        BSP_GPIO_595_SHCP_Set();
        val <<= 1;
    }

    hc595_shadow = data;
}

void HC595_SetBit(uint8_t chip, uint8_t bit)
{
    if (bit < 1 || bit > 7) return;
    if (chip == 1) {
        hc595_shadow |=  (1UL << (bit - 1));
    } else if (chip == 2) {
        hc595_shadow |=  (1UL << (bit + 7));
    }
    HC595_Write(hc595_shadow);
}

void HC595_ClrBit(uint8_t chip, uint8_t bit)
{
    if (bit < 1 || bit > 7) return;
    if (chip == 1) {
        hc595_shadow &= ~(1UL << (bit - 1));
    } else if (chip == 2) {
        hc595_shadow &= ~(1UL << (bit + 7));
    }
    HC595_Write(hc595_shadow);
}

void HC595_ClearAll(void)
{
    hc595_shadow = 0x0000;
    HC595_Write(0x0000);
}

/* ================================================================
 * Control Signals
 * ================================================================ */

void HC595_Latch(void)
{
    BSP_GPIO_595_STCP_Set();
    /* Small delay for setup */
    for (volatile int i = 0; i < 10; i++) {}
    BSP_GPIO_595_STCP_Clr();
}

void HC595_Reset(void)
{
    BSP_GPIO_595_MR_Clr();
    for (volatile int i = 0; i < 10; i++) {}
    BSP_GPIO_595_MR_Set();
}

void HC595_OutputEnable(bool enable)
{
    if (enable)
        BSP_GPIO_595_OE_Clr();   /* OE low = outputs enabled */
    else
        BSP_GPIO_595_OE_Set();   /* OE high = Hi-Z */
}
