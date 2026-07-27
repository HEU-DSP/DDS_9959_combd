/**
 ******************************************************************************
 * @file    hc595.h
 * @brief   74HC595 Dual Cascaded Shift-Register Driver
 *
 * Hardware:
 *   PE2 → STCP (RCLK) — storage register clock (latch)
 *   PE3 → DS (SER)    — serial data in (via USART10 half-duplex TX)
 *   PE4 → SHCP (SRCLK) — shift register clock
 *   PE5 → MR (SRCLR)   — master reset (active low)
 *   PE6 → OE           — output enable (active low)
 *
 * Chip 1 (closest to MCU):  7 LED outputs (bits 1-7, bit 0 NC)
 * Chip 2 (cascaded from QH'): 6 DDS control outputs (Q1-Q6; Q0/Q7 NC)
 ******************************************************************************
 */

#ifndef __HC595_H__
#define __HC595_H__

#include <stdint.h>
#include <stdbool.h>

/* ================================================================
 * Chip 1 Bit Definitions (LED indicators)
 * ================================================================ */
#define HC595_CH1_BIT_LEDSTBY            1
#define HC595_CH1_BIT_LED_ANALOGREADY    2
#define HC595_CH1_BIT_LEDMODREADY        3
#define HC595_CH1_BIT_LEDCH0TRANSMIT     4
#define HC595_CH1_BIT_LEDCH1TRANSMIT     5
#define HC595_CH1_BIT_LEDCH2TRANSMIT     6
#define HC595_CH1_BIT_LEDCH3TRANSMIT     7

/* ================================================================
 * Chip 2 Bit Definitions (DDS Control)
 * ================================================================ */
#define HC595_CH2_BIT_DDSPWREN1V8D       1
#define HC595_CH2_BIT_DDSPWREN1V8A       2
#define HC595_CH2_BIT_DDSMASTERRST       3
#define HC595_CH2_BIT_DDSPDN             4
#define HC595_CH2_BIT_DDSPWREN_3V3D      5
#define HC595_CH2_BIT_DDSCLKMODE33       6

/* Logical DDS control state. The board has mixed signal polarities; use
 * HC595_ApplyDDSControl() instead of manipulating chip-2 bit masks directly. */
typedef struct {
    bool enable_1v8_digital;
    bool enable_1v8_analog;
    bool master_reset;
    bool power_down;
    bool enable_3v3_digital;
    bool clk_mode_3v3;
} HC595_DDSControl;

extern HC595_DDSControl hc595_dds_control;

/* ================================================================
 * API
 * ================================================================ */

/**
 * @brief  Initialize 595 control GPIOs
 */
void HC595_Init(void);

/**
 * @brief  Write full 16-bit value (chip1[7:0] | chip2[15:8])
 * @param  data : [15:8]=chip2 shift-reg, [7:0]=chip1 shift-reg
 * @note   Data is shifted MSB-first via GPIO and automatically latched.
 */
void HC595_Write(uint16_t data);

/**
 * @brief  Set a bit in the specified chip's shadow register
 * @param  chip : 1 or 2
 * @param  bit  : 1–7
 */
void HC595_SetBit(uint8_t chip, uint8_t bit);

/**
 * @brief  Clear a bit in the specified chip's shadow register
 */
void HC595_ClrBit(uint8_t chip, uint8_t bit);

/**
 * @brief  Clear all bits in both chips' shadow registers
 */
void HC595_ClearAll(void);

/** Apply hc595_dds_control to the second 74HC595 and latch its outputs. */
void HC595_ApplyDDSControl(void);

/**
 * @brief  Pulse STCP (latch) — transfer shift register to outputs
 */
void HC595_Latch(void);

/**
 * @brief  Pulse MR (reset) — clear shift registers
 */
void HC595_Reset(void);

/**
 * @brief  Enable/disable parallel outputs
 * @param  enable : true = OE low (outputs active), false = OE high (Hi-Z)
 */
void HC595_OutputEnable(bool enable);

/**
 * @brief  批量更新 Chip1 LED 位（保持 Chip2 DDS 控制不变）
 * @param  leds : bit0=STBY, bit1=ANALOG, bit2=MODREADY, bit3=CH0,
 *                bit4=CH1, bit5=CH2, bit6=CH3. 1=亮, 0=灭.
 * @note   一次 HC595_Write 完成，适合 ISR 调用.
 */
void HC595_WriteLEDs(uint8_t leds);

#endif /* __HC595_H__ */
