/**
 ******************************************************************************
 * @file    phase1_config.h
 * @brief   Phase 1 Verification Configuration
 *
 * Centralised test parameters — not spread across main.c.
 ******************************************************************************
 */

#ifndef __PHASE1_CONFIG_H__
#define __PHASE1_CONFIG_H__

#include "mod_fsk.h"
#include "mod_ask.h"

/* ================================================================
 * AD9959 System Clock
 * ================================================================ */
#define AD9959_SYSCLK_HZ   500000000UL   /* 500 MHz (PLL x20, REF=50MHz) */

/* ================================================================
 * FTW constants (FTW = f_out / SYSCLK * 2^32)
 *   10 MHz →  85899346 (0x051EB852)
 *   11 MHz →  94489280 (0x05A1CAC0)
 * ================================================================ */
#define FTW_10MHZ  85899346UL
#define FTW_11MHZ  94489280UL

/* ================================================================
 * Phase 1 default baud rate + oversampling (App layer sets these)
 * sample_rate = baud_rate × samples_per_sym
 *   CW/FSK/ASK:  samples_per_sym = 1
 *   GFSK:        samples_per_sym = 4
 * ================================================================ */
#define P1_BAUD_RATE            100000U   /* 100 kHz symbol rate               */
#define P1_SAMPLES_PER_SYM      1U        /* No oversampling for CW/FSK/ASK     */
#define P1_CH1_DELAY            200U      /* TIM8 CH1 initial delay ticks       */
#define P1_CH2_DELAY            210U      /* TIM8 CH2 initial delay ticks       */

/* ================================================================
 * Frame transfer time constraint
 * SPI baud = 125 Mbps, DataSize = 4-bit
 * 1 byte = 4 SCLK × 8 ns = 32 ns transfer time
 * Max bytes = FRAME_TRANSFER_TIME_US * 1000 / 32
 *   5 µs → 156 bytes max (current encoder: ~14 bytes per command)
 * ================================================================ */
#define FRAME_TRANSFER_TIME_US    5U       /* Target max SPI busy time per frame */
#define FRAME_MAX_BYTES          ((FRAME_TRANSFER_TIME_US * 1000UL) / 32)

#endif /* __PHASE1_CONFIG_H__ */
