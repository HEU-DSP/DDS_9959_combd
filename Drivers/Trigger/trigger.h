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
 * SPI3 (dual-wire 2-bit mode) is deferred.
 ******************************************************************************
 */

#ifndef __TRIGGER_H__
#define __TRIGGER_H__

#include <stdint.h>
#include "main.h"       /* HAL_StatusTypeDef */

typedef struct {
    const uint8_t  *spi1_ping;     /* boot-time DMA source (bank 0) */
    uint16_t        bank_size;     /* boot-time bytes per frame     */
    uint32_t        sample_rate;   /* Tx frame rate (Hz) */
    uint16_t        ch1_delay;     /* TIM8 CH1 (IO_UPDATE) ticks */
    uint16_t        ch2_delay;     /* TIM8 CH2 (DIO3) ticks */
} Trigger_Config;

/* Bounded polls (DWT-delay based, ~30 ms ceiling) used to keep the chain
 * transitions synchronous — see Trigger_Stop(). */
#define TRIGGER_ABORT_TIMEOUT_MS     30U
#define TRIGGER_ARM_TIMEOUT_MS       10U

void Trigger_Init(const Trigger_Config *cfg);

/**
 * @brief  Arm the DMA chain and start LPTIM3.
 * @return HAL_OK if armed and started; HAL_ERROR/HAL_BUSY if the DMA could not
 *         be armed — LPTIM3 is then left STOPPED so no overrun can occur.
 */
HAL_StatusTypeDef Trigger_Start(void);

/**
 * @brief  Stop LPTIM3/TIM8/TIM4 and abort the SPI1 DMA, then wait for SPI1 to
 *         reach READY.  Synchronous: returns with the transport fully idle.
 * @return HAL_OK if SPI1 reached READY; HAL_TIMEOUT otherwise.
 */
HAL_StatusTypeDef Trigger_Stop(void);

/**
 * @brief  Resume after Trigger_Stop() without a full re-init.
 * @return Same convention as Trigger_Start().
 */
HAL_StatusTypeDef Trigger_Restart(void);

/**
 * @brief  Swap the ping-pong bank and re-arm the DMA mid-stream.
 * @return Same convention as Trigger_Start().  The caller must NOT rely on the
 *         swap having taken effect when this returns non-HAL_OK.
 */
HAL_StatusTypeDef Trigger_SwapBuffer(void);

#endif /* __TRIGGER_H__ */
