/**
 ******************************************************************************
 * @file    led_indicator.c
 * @brief   LED indicator driver — state update (main loop) + refresh (TIM5 ISR)
 *
 * Two halves:
 *  - Led_UpdateTxState(): main-loop context, derives leds.mod_ready and the
 *    four channel LEDs from mod_cfg[], the transport chain state and the
 *    symbol buffer.  Runs once per main-loop pass.
 *  - Led_Refresh(): TIM5 10 Hz ISR, pure state→output mapper.  Reads the
 *    leds struct and pushes chip-1 bits to the 74HC595.  It never reads
 *    mod_cfg / sym_buf / ad9959_diag itself.
 ******************************************************************************
 */

#include "led_indicator.h"
#include "hc595.h"
#include "mod_config.h"
#include "symbol_buffer.h"
#include "debug_state.h"

volatile LedIndicator leds;
static volatile uint32_t led_tick;

/* The 74HC595 chip-1 Q4..Q7 channel LEDs, in channel order, so the channel
 * loop in Led_UpdateTxState() stays table-driven. */
static volatile LedState *const ch_led[DDS_CHANNEL_COUNT] = {
    &leds.ch0_transmit,
    &leds.ch1_transmit,
    &leds.ch2_transmit,
    &leds.ch3_transmit,
};

/* ASF is a 10-bit field.  Mirrors ACR_ASF_Msk applied by the encoder
 * (dds_encoder.c); the non-CW SET commands do NOT range-check their asf
 * payloads, so unmasked values can silently exceed 10 bits. */
#define LED_ASF_MASK  0x3FFU

/**
 * @brief  Per-channel transmit amplitude (10-bit ASF) for the LED decision.
 *
 * ASK/AM carry two amplitude fields; any non-zero one counts as "emitting".
 * CH_MODE_OFF and unrecognized modes return 0.
 *
 * @param  ch  channel index (0..DDS_CHANNEL_COUNT-1)
 * @return masked ASF, 0 = silent
 */
static uint16_t led_ch_amplitude(uint8_t ch)
{
    const ChannelModConfig *c = &mod_cfg[ch];
    uint16_t amp;

    switch ((ChMode)c->mode) {
    case CH_MODE_CW:   amp = c->cw.asf;                          break;
    case CH_MODE_FSK:  amp = c->fsk.asf;                         break;
    case CH_MODE_ASK:  amp = (uint16_t)(c->ask.asf_on | c->ask.asf_off); break;
    case CH_MODE_GFSK: amp = c->gfsk.asf;                        break;
    case CH_MODE_MSK:  amp = c->msk.asf;                         break;
    case CH_MODE_QPSK: amp = c->qpsk.asf;                        break;
    case CH_MODE_AM:   amp = (uint16_t)(c->am.asf_center | c->am.asf_delta); break;
    case CH_MODE_FM:   amp = c->fm.asf;                          break;
    case CH_MODE_BPSK: amp = c->bpsk.asf;                        break;
    case CH_MODE_4FSK: amp = c->fsk4.asf;                        break;
    default:           amp = 0U;                                 break;
    }
    return (uint16_t)(amp & LED_ASF_MASK);
}

/**
 * @brief  Update the TX-status LEDs from live system state.
 *
 * Channel LEDs: off when the channel is disabled; 1 Hz blink when enabled
 * but silent (ASF = 0); steady on when enabled with a non-zero amplitude.
 *
 * MOD READY: on when the transport chain is running and no symbol payload
 * is pending; off while a received data frame's symbols are still being
 * transmitted, and off whenever the chain is stopped.
 *
 * Must run once per main-loop pass, OUTSIDE the dma_bank_ready block:
 * SymbolBuf_HasData() is transiently true between SymbolSource_Generate()
 * and Encoder_BuildBank_DMA(), and only the end-of-pass state is stable.
 * Never called from an ISR — Led_Refresh() stays the only ISR entry point.
 */
void Led_UpdateTxState(void)
{
    for (uint8_t ch = 0U; ch < DDS_CHANNEL_COUNT; ch++) {
        if (!mod_cfg[ch].enabled) {
            *ch_led[ch] = LED_OFF;
        } else if (led_ch_amplitude(ch) == 0U) {
            *ch_led[ch] = LED_BLINK_1HZ;
        } else {
            *ch_led[ch] = LED_ON;
        }
    }

    leds.mod_ready = ((ad9959_diag.tx_running != 0U)
                      && !SymbolBuf_HasData() && !SymbolBuf_IsStaged())
                     ? LED_ON : LED_OFF;
}

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
