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
    LedState stby;           /* ST-BY 指示灯            */
    LedState analog_ready;   /* ANALOG READY 指示灯     */
    LedState mod_ready;      /* MOD READY 指示灯        */
    LedState ch0_transmit;   /* CH0 发射指示            */
    LedState ch1_transmit;   /* CH1 发射指示            */
    LedState ch2_transmit;   /* CH2 发射指示            */
    LedState ch3_transmit;   /* CH3 发射指示            */
} LedIndicator;

extern volatile LedIndicator leds;

/**
 * @brief  TIM5 ISR 调用，10 Hz 刷新 LED 状态.
 */
void Led_Refresh(void);

#endif /* __LED_INDICATOR_H__ */
