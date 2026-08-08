/**
 ******************************************************************************
 * @file    trigger.h
 * @brief   LPTIM3→DMAMUX→SPI DMA→TIM8 Hardware Trigger Chain
 *
 * Architecture (DMA mode, AD9959_DOWNGRADE_MODE = 0):
 *   LPTIM3 (PA1 = OUT, 135 MHz / DIV1)
 *     |  LPTIM3_OUT periodic pulse
 *     +──→ DMAMUX Sync Gate ──→ SPI1 TX DMA unlock
 *     |        (RequestNumber = 14 bytes per pulse = one channel)
 *     +──→ 飞线 PA0 (TIM8_ETR) ──→ TIM8 hardware reset
 *     |        +──→ CH1 PWM (PC6) = IO_UPDATE pulse (CCR1 delay)
 *     |        +──→ CH2 PWM (PC7) = DIO3 pulse (CCR2 delay)
 *     +──→ 飞线 PE0 (TIM4_ETR) ──→ TIM4 hardware reset
 *               +──→ CH1 (PB6) = CS↓ capture, CH2 (PB7) = CS↑ capture
 *
 * DMA: one LPTIM3_OUT pulse → gate open → 14 bytes (1 channel) sent;
 *      the DMA pauses at the next sync boundary until the following pulse.
 *      Four pulses complete a 4-channel frame (56 bytes) fully in hardware.
 *      TC ISR then swaps the ping-pong bank and re-arms the DMA.
 *
 * TIM2 is no longer part of the chain (removed).
 * SPI3 (dual-wire 2-bit mode) is deferred; spi3 pointers are unused for now.
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
