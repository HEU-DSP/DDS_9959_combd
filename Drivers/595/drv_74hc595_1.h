/**
  ******************************************************************************
  * @file    drv_74hc595_1.h
  * @brief   Driver for two daisy-chained 74HC595 shift registers
  *
  * Pin mapping (from IOC / main.h):
  *   PE2 - OCR_STCP   (Storage/Latch clock)
  *   PE3 - OCR_DS     (Serial data, via USART10 TX or GPIO)
  *   PE4 - OCR_SHCP   (Shift clock)
  *   PE5 - OCR_NMR    (Master reset, active low)
  *   PE6 - OCR_NOE    (Output enable, active low)
  *
  * Cascade order: PE3 → Chip1 DS → Chip1 Q7' → Chip2 DS
  *   Chip 1 (first to receive, bits [7:0])  — Status LEDs
  *   Chip 2 (cascaded,      bits [15:8]) — DDS control
  *
  * Reference: DOCS/引脚对照表.md "OCR 控制接口"
  ******************************************************************************
  */
#ifndef __DRV_74HC595_1_H
#define __DRV_74HC595_1_H

#include "main.h"
#include <stdint.h>

/* ================================================================
 * Pin aliases — values from main.h (CubeMX generated)
 * ================================================================ */
#define OCR_DS_PORT     OCR_DS_GPIO_Port       /* PE3 */
#define OCR_DS_PIN      OCR_DS_Pin
#define OCR_SHCP_PORT   OCR_SHCP_GPIO_Port     /* PE4 */
#define OCR_SHCP_PIN    OCR_SHCP_Pin
#define OCR_STCP_PORT   OCR_STCP_GPIO_Port     /* PE2 */
#define OCR_STCP_PIN    OCR_STCP_Pin
#define OCR_MR_PORT     OCR_NMR_GPIO_Port      /* PE5 — MR# active low */
#define OCR_MR_PIN      OCR_NMR_Pin
#define OCR_OE_PORT     OCR_NOE_GPIO_Port      /* PE6 — OE# active low */
#define OCR_OE_PIN      OCR_NOE_Pin

/* ================================================================
 * Bit definitions — Chip 1 (bits [7:0]): Status LEDs
 *
 *   Bit 0: NC
 * ================================================================ */
#define SHIFTREG_LED_STBY          (1U << 1)   /* Q1: Standby indicator       */
#define SHIFTREG_LED_ANALOGREADY   (1U << 2)   /* Q2: Analog frontend ready   */
#define SHIFTREG_LED_MODREADY      (1U << 3)   /* Q3: Modulation ready        */
#define SHIFTREG_LED_CH0TRANSMIT   (1U << 4)   /* Q4: CH0 transmitting        */
#define SHIFTREG_LED_CH1TRANSMIT   (1U << 5)   /* Q5: CH1 transmitting        */
#define SHIFTREG_LED_CH2TRANSMIT   (1U << 6)   /* Q6: CH2 transmitting        */
#define SHIFTREG_LED_CH3TRANSMIT   (1U << 7)   /* Q7: CH3 transmitting        */

#define SHIFTREG_LED_ALL  (0x00FEU)   /* bits 1-7 */

/* ================================================================
 * Bit definitions — Chip 2 (bits [15:8]): DDS Control
 *
 *   Bit 8  (Chip2 Q0): NC
 *   Bit 15 (Chip2 Q7): NC
 * ================================================================ */
#define SHIFTREG_DDS_PWREN_1V8D   (1U << 9)   /* Q1: DDS digital 1.8V enable  */
#define SHIFTREG_DDS_PWREN_1V8A   (1U << 10)  /* Q2: DDS analog  1.8V enable  */
#define SHIFTREG_DDS_MASTERRST    (1U << 11)  /* Q3: DDS master reset         */
#define SHIFTREG_DDS_PDN          (1U << 12)  /* Q4: DDS power-down           */
#define SHIFTREG_DDS_PWREN_3V3D   (1U << 13)  /* Q5: DDS digital 3.3V enable  */
#define SHIFTREG_DDS_CLKMODE33    (1U << 14)  /* Q6: DDS REF_CLK mode (3.3V)  */

#define SHIFTREG_DDS_ALL  (0x7E00U)   /* bits 9-14 */

/* ---- API ---- */

void DRV_595_Init(void);
void DRV_595_Write(uint16_t data);
void DRV_595_SetBit(uint16_t bit_mask, uint8_t value);
void DRV_595_EnableOutput(uint8_t enable);
void DRV_595_Reset(void);
void DRV_595_PowerSeq_Init(void);
void DRV_595_PowerSeq_Sleep(void);

extern uint16_t g_shiftreg_state;

/* ---- Named bitfield overlay for debugger watch ---- */
typedef union {
    uint16_t raw;
    struct {
        /* Chip 1: LEDs [7:0] */
        uint16_t nc_ch1_q0         : 1;   /* bit 0:  NC */
        uint16_t led_stby           : 1;   /* bit 1:  LEDSTBY */
        uint16_t led_analogready    : 1;   /* bit 2:  LED ANALOGREADY */
        uint16_t led_modready       : 1;   /* bit 3:  LEDMODREADY */
        uint16_t led_ch0transmit    : 1;   /* bit 4:  LEDCH0TRANSMIT */
        uint16_t led_ch1transmit    : 1;   /* bit 5:  LEDCH1TRANSMIT */
        uint16_t led_ch2transmit    : 1;   /* bit 6:  LEDCH2TRANSMIT */
        uint16_t led_ch3transmit    : 1;   /* bit 7:  LEDCH3TRANSMIT */
        /* Chip 2: DDS control [15:8] */
        uint16_t nc_ch2_q0         : 1;   /* bit 8:  NC */
        uint16_t dds_pwren_1v8d    : 1;   /* bit 9:  DDSPWREN1V8D */
        uint16_t dds_pwren_1v8a    : 1;   /* bit 10: DDSPWREN1V8A */
        uint16_t dds_masterrst     : 1;   /* bit 11: DDSMASTERRST */
        uint16_t dds_pdn           : 1;   /* bit 12: DDSPDN */
        uint16_t dds_pwren_3v3d    : 1;   /* bit 13: DDSPWREN_3V3D */
        uint16_t dds_clkmode33     : 1;   /* bit 14: DDSCLKMODE33 */
        uint16_t nc_ch2_q7         : 1;   /* bit 15: NC */
    } bits;
} ShiftReg_State_t;

extern volatile ShiftReg_State_t g_595;

#endif /* __DRV_74HC595_1_H */
