/**
 ******************************************************************************
 * @file    led_indicator.h
 * @brief   LED indicator state machine — TIM5 10 Hz ISR driven
 ******************************************************************************
 */

#ifndef __LED_INDICATOR_H__
#define __LED_INDICATOR_H__

#include <stdint.h>

typedef enum {
    LED_OFF      = 0,
    LED_ON       = 1,
    LED_BLINK_1HZ = 2
} LedState;

typedef struct {
    LedState stby;           /* ST-BY 指示灯                          */
    LedState analog_ready;   /* ANALOG READY 指示灯                   */
    LedState mod_ready;      /* 调制就绪：传输链运行且无待发符号数据时亮 */
    LedState ch0_transmit;   /* CH0：使能闪烁 / 幅度非零常亮 / 不使能灭 */
    LedState ch1_transmit;   /* CH1：同上                              */
    LedState ch2_transmit;   /* CH2：同上                              */
    LedState ch3_transmit;   /* CH3：同上                              */
} LedIndicator;

extern volatile LedIndicator leds;

/**
 * @brief  TIM5 ISR 调用，10 Hz 刷新 LED 状态.
 */
void Led_Refresh(void);

/**
 * @brief  从实时系统状态更新 TX 状态灯（mod_ready + CH0..CH3）。
 *
 * 主循环每 pass 调用一次，**不可从 ISR 调用**。必须在 dma_bank_ready
 * 块之外调用：SymbolBuf_HasData() 在 Generate 与 BuildBank 之间瞬时为
 * 真，只有 pass 末状态是稳定的。
 *
 * 通道灯：不使能灭 / 使能且 ASF=0 闪烁 / 使能且幅度非零常亮。
 * MOD READY：传输链运行中且无待发符号数据时亮。
 */
void Led_UpdateTxState(void);

#endif /* __LED_INDICATOR_H__ */
