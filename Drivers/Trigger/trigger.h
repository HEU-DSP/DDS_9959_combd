/**
 ******************************************************************************
 * @file    trigger.h
 * @brief   LPTIM3→DMA→SPI→TIM8 Hardware Trigger Chain
 *
 * Architecture:
 *   LPTIM3_OUT (PA1) ─┬→ DMAMUX Sync Gate ──→ SPI1+SPI3 DMA unlock
 *                      ├→ fly-wire PA0 → TIM8 ETR → TIM8 reset → CH1/CH2 pulse
 *                      └→ fly-wire PE0 → TIM4 ETR → TIM4 counter reset
 *
 *   One LPTIM3_OUT period → gate open → entire SPI bank sent → CS↓→data→CS↑
 *   → TIM8 CH1/CH2 delayed pulses → IO_UPDATE/DIO3 → one TC ISR per bank.
 *
 *   LPTIM3 clock: 135 MHz (D3PCLK1), prescaler /1
 *   TIM8 clock:   135 MHz (APB2)
 *   TIM4 clock:   135 MHz (APB1)
 ******************************************************************************
 */

#ifndef __TRIGGER_H__
#define __TRIGGER_H__

#include <stdint.h>

typedef struct {
    const uint8_t  *spi1_ping;
    const uint8_t  *spi3_ping;
    const uint8_t  *spi1_pong;
    const uint8_t  *spi3_pong;
    uint16_t        bank_size;
    uint32_t        sample_rate;   /* Tx frame rate (Hz) */
    uint16_t        ch1_delay;     /* TIM8 CH1 (IO_UPDATE) ticks */
    uint16_t        ch2_delay;     /* TIM8 CH2 (DIO3) ticks */
} Trigger_Config;

void Trigger_Init(const Trigger_Config *cfg);
void Trigger_Start(void);
void Trigger_Stop(void);
void Trigger_Restart(void);   /* Resume after Stop without full re-init */
void Trigger_SwapBuffer(void);
void Trigger_GetCaptureTiming(uint32_t *t_cs_start, uint32_t *t_cs_end);

#endif /* __TRIGGER_H__ */
