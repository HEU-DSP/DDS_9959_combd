/**
 ******************************************************************************
 * @file    led_indicator.c
 * @brief   LED indicator refresh — called from TIM5 10 Hz ISR
 ******************************************************************************
 */

#include "led_indicator.h"
#include "hc595.h"

volatile LedIndicator leds;
static volatile uint32_t led_tick;

static uint8_t led_state_to_output(LedState state, bool blink_on)
{
    switch (state) {
    case LED_ON:
        return 1U;
    case LED_BLINK_1HZ:
        return blink_on ? 1U : 0U;
    default:
        return 0U;
    }
}

void Led_Refresh(void)
{
    led_tick++;

    /* 1 Hz blink phase: 10 ticks/s, 5 ticks on, 5 ticks off */
    bool blink_on = ((led_tick % 10U) < 5U);

    uint8_t out = 0U;
    if (led_state_to_output(leds.stby,           blink_on)) out |= 0x01U;
    if (led_state_to_output(leds.analog_ready,   blink_on)) out |= 0x02U;
    if (led_state_to_output(leds.mod_ready,      blink_on)) out |= 0x04U;
    if (led_state_to_output(leds.ch0_transmit,   blink_on)) out |= 0x08U;
    if (led_state_to_output(leds.ch1_transmit,   blink_on)) out |= 0x10U;
    if (led_state_to_output(leds.ch2_transmit,   blink_on)) out |= 0x20U;
    if (led_state_to_output(leds.ch3_transmit,   blink_on)) out |= 0x40U;

    HC595_WriteLEDs(out);
}
