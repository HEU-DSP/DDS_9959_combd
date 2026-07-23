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
 *   Q0=NC, Q1=LEDSTBY, ..., Q7=LEDCH3TRANSMIT
 *   Ref: DOCS/引脚对照表.md "第一片 595 并行输出"
 * ================================================================ */
#define SHIFTREG_LED_STBY          (1U << 1)   /* Q1: LEDSTBY          */
#define SHIFTREG_LED_ANALOGREADY   (1U << 2)   /* Q2: LED ANALOGREADY  */
#define SHIFTREG_LED_MODREADY      (1U << 3)   /* Q3: LEDMODREADY      */
#define SHIFTREG_LED_CH0TRANSMIT   (1U << 4)   /* Q4: LEDCH0TRANSMIT   */
#define SHIFTREG_LED_CH1TRANSMIT   (1U << 5)   /* Q5: LEDCH1TRANSMIT   */
#define SHIFTREG_LED_CH2TRANSMIT   (1U << 6)   /* Q6: LEDCH2TRANSMIT   */
#define SHIFTREG_LED_CH3TRANSMIT   (1U << 7)   /* Q7: LEDCH3TRANSMIT   */

#define SHIFTREG_LED_ALL  (0x00FEU)   /* bits 1-7 */

/* ================================================================
 * Bit definitions — Chip 2 (bits [15:8]): DDS Control
 *   Q0=NC, Q1=DDSPWREN1V8D, ..., Q6=DDSCLKMODE33, Q7=NC
 *   Ref: DOCS/引脚对照表.md "第二片 595 并行输出"
 * ================================================================ */
#define SHIFTREG_DDS_PWREN_1V8D   (1U << 9)   /* Q1: DDSPWREN1V8D    */
#define SHIFTREG_DDS_PWREN_1V8A   (1U << 10)  /* Q2: DDSPWREN1V8A    */
#define SHIFTREG_DDS_MASTERRST    (1U << 11)  /* Q3: DDSMASTERRST    */
#define SHIFTREG_DDS_PDN          (1U << 12)  /* Q4: DDSPDN           */
#define SHIFTREG_DDS_PWREN_3V3D   (1U << 13)  /* Q5: DDSPWREN_3V3D   */
#define SHIFTREG_DDS_CLKMODE33    (1U << 14)  /* Q6: DDSCLKMODE33    */

#define SHIFTREG_DDS_ALL  (0x7E00U)   /* bits 9-14 */

/* ---- API ---- */

void DRV_595_Init(void);
void DRV_595_Write(uint16_t data);
void DRV_595_SetBit(uint16_t bit_mask, uint8_t value);
void DRV_595_EnableOutput(uint8_t enable);
void DRV_595_Reset(void);
void DRV_595_PowerSeq_Init(void);
void DRV_595_PowerSeq_Sleep(void);
void DRV_595_Refresh(void);

extern uint16_t g_shiftreg_state;

/* ================================================================
 * Convenience macros — use with g_595.raw = LED_ALL_ON | DDS_NORMAL;
 * ================================================================ */
#define BIT_STBY        SHIFTREG_LED_STBY
#define BIT_ANALOGRDY   SHIFTREG_LED_ANALOGREADY
#define BIT_MODRDY      SHIFTREG_LED_MODREADY
#define BIT_CH0TX       SHIFTREG_LED_CH0TRANSMIT
#define BIT_CH1TX       SHIFTREG_LED_CH1TRANSMIT
#define BIT_CH2TX       SHIFTREG_LED_CH2TRANSMIT
#define BIT_CH3TX       SHIFTREG_LED_CH3TRANSMIT
#define LED_ALL_ON      SHIFTREG_LED_ALL
#define LED_ALL_OFF     0x0000U

#define BIT_1V8D        SHIFTREG_DDS_PWREN_1V8D
#define BIT_1V8A        SHIFTREG_DDS_PWREN_1V8A
#define BIT_MRST        SHIFTREG_DDS_MASTERRST
#define BIT_PDN         SHIFTREG_DDS_PDN
#define BIT_3V3D        SHIFTREG_DDS_PWREN_3V3D
#define BIT_CLKM33      SHIFTREG_DDS_CLKMODE33
#define DDS_NORMAL_RUN  (BIT_1V8D | BIT_1V8A | BIT_3V3D | BIT_CLKM33 | BIT_MRST)
#define DDS_ALL_OFF     0x0000U

/* ---- Named bitfield overlay for debugger watch ---- */
typedef union {
    uint16_t raw;
    struct {
        /* Chip 1: LEDs — Q0=NC, Q1~Q7 active */
        uint16_t _nc0     : 1;   /* bit 0:  NC (Chip1 Q0) */
        uint16_t l_stby   : 1;   /* bit 1:  LEDSTBY */
        uint16_t l_ard    : 1;   /* bit 2:  LED ANALOG READY */
        uint16_t l_mrd    : 1;   /* bit 3:  LED MOD READY */
        uint16_t l_c0tx   : 1;   /* bit 4:  CH0 TRANSMIT */
        uint16_t l_c1tx   : 1;   /* bit 5:  CH1 TRANSMIT */
        uint16_t l_c2tx   : 1;   /* bit 6:  CH2 TRANSMIT */
        uint16_t l_c3tx   : 1;   /* bit 7:  CH3 TRANSMIT */
        /* Chip 2: DDS — Q0=NC, Q1~Q6 active, Q7=NC */
        uint16_t _nc8     : 1;   /* bit 8:  NC (Chip2 Q0) */
        uint16_t d_1v8d   : 1;   /* bit 9:  DDSPWREN1V8D */
        uint16_t d_1v8a   : 1;   /* bit 10: DDSPWREN1V8A */
        uint16_t d_mrst   : 1;   /* bit 11: DDSMASTERRST */
        uint16_t d_pdn    : 1;   /* bit 12: DDSPDN */
        uint16_t d_3v3d   : 1;   /* bit 13: DDSPWREN_3V3D */
        uint16_t d_clkm   : 1;   /* bit 14: DDSCLKMODE33 */
        uint16_t _nc15    : 1;   /* bit 15: NC (Chip2 Q7) */
    } bits;
} ShiftReg_State_t;

extern volatile ShiftReg_State_t g_595;

#endif /* __DRV_74HC595_1_H */
