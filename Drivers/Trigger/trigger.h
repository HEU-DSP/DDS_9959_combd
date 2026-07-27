/**
 ******************************************************************************
 * @file    trigger.h
 * @brief   LPTIM1→DMA→SPI→TIM8 Hardware Trigger Chain
 *
 * Architecture:
 *   LPTIM1_OUT ──→ DMAMUX Sync Gate ──→ SPI1+SPI3 DMA unlock
 *                                    ──→ CS↓ → SCLK → DATA
 *   TIM2 CH1 ↑──→ ETRF reset TIM2 → Update → ITR1 → TIM8 reset
 *              └──→ (飞线 PE0) → TIM4 ETR reset
 *   TIM8 CH1/CH2 → delayed pulse → IO_UPDATE / DIO3
 *
 * DMA: one LPTIM1_OUT → gate open → entire bank sent → CS↓→data→CS↑
 *       → TIM8 IO_UPDATE pulse → one TC ISR per bank.
 * LPTIM1 prescaler: /8 → 16 MHz tick
 * TIM2 clock: 256 MHz (APB2)
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
void Trigger_PauseAtFrameBoundary(void);
void Trigger_Restart(void);   /* Resume after Stop without full re-init */
void Trigger_SwapBuffer(void);
void Trigger_GetCaptureTiming(uint32_t *t_cs_start, uint32_t *t_cs_end);

#endif /* __TRIGGER_H__ */
