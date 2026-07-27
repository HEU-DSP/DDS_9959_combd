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
#include "dds_calc.h"

/* ================================================================
 * AD9959 System Clock — delegated to DDSCALC_AD9959_SYSCLK_HZ
 * ================================================================ */
#define AD9959_SYSCLK_HZ   DDSCALC_AD9959_SYSCLK_HZ

/* ================================================================
 * Transport mode
 *   1 = downgrade: TIM8 periodic ISR + single-wire SPI1 (no DMA)
 *   0 = original:  LPTIM3 → DMA → SPI1+SPI3 → TIM2 → TIM8 chain
 * ================================================================ */
#define AD9959_DOWNGRADE_MODE  1

/* Bring-up mode: program CH0 CW entirely through the one-bit DIO0 protocol. */
#define AD9959_DIRECT_CW_TEST  0

/* Transport selector for the direct-CW test:
 * 0 = validated hardware SPI1 transport (default)
 * 1 = GPIO bit-banged CS/SCLK/DIO0 at a deliberately slow rate. */
#define AD9959_SOFTWARE_SPI_TEST  0

/* Reproduce the official module's per-channel write ordering. */
#define AD9959_OFFICIAL_PER_CHANNEL_TEST  1

/* Hardware-SPI1 DMA CFTW loop test.  Requires AD9959_SOFTWARE_SPI_TEST = 0. */
#define AD9959_CYCLIC_FTW_DMA_TEST  0
#define AD9959_CYCLIC_FTW_PERIOD_MS 10U

/* Read CSR/FR1/CFR/CFTW/ACR back through DIO0 after direct-CW setup.
 * Results are retained in ad9959_onebit_debug for Ozone inspection. */
#define AD9959_DIRECT_CW_READBACK  0

/* Optional physical-layer test. DDS stays in reset while SPI1 repeatedly
 * sends 00 AA / 00 55, making SDIO0 straightforward to probe. */
#define AD9959_SPI_WAVEFORM_TEST  0

/* Hardware-only 74HC595 check: both chips' Q1..Q7 stay high. */
#define HC595_OUTPUT_SELFTEST  0
#define HC595_OUTPUT_SELFTEST_WORD  0xFEFEU

/* ================================================================
 * FTW constants (FTW = f_out / SYSCLK * 2^32)
 *   10 MHz →  85899346 (0x051EB852)
 *   11 MHz →  94489280 (0x05A1CAC0)
 * ================================================================ */
#define FTW_10MHZ  87490075UL
#define FTW_11MHZ  96239082UL
/* 200 MHz at SYSCLK = 490.909091 MHz: 0x684BDA13. */
#define FTW_200MHZ 1749801491UL
/* 100.3 MHz at SYSCLK = 490.909091 MHz: 0x344DF9C8. */
#define FTW_100P3MHZ 877525448UL
/* 99.7 MHz at SYSCLK = 490.909091 MHz: 0x33F9C9A7. */
#define FTW_99P7MHZ  872226471UL

/* ================================================================
 * Phase 1 default baud rate + oversampling (App layer sets these)
 * sample_rate = baud_rate × samples_per_sym
 *   CW/FSK/ASK:  samples_per_sym = 1
 *   GFSK:        samples_per_sym = 4
 * ================================================================ */
#define P1_BAUD_RATE            100000U   /* validated DMA sample rate          */
#define P1_SAMPLES_PER_SYM      1U        /* No oversampling for CW/FSK/ASK     */
#define P1_CH1_DELAY            200U      /* TIM8 CH1 initial delay ticks       */
#define P1_CH2_DELAY            210U      /* TIM8 CH2 initial delay ticks       */

/* TIM4 is a measurement-only CS capture path in phase 1.  Enable only when:
 *   PA15 (AD9959 CS) -> PB6 (TIM4_CH1) and PB7 (TIM4_CH2)
 *   PA0  (TIM2_CH1)  -> PE0 (TIM4_ETR)
 * MONITOR reads CCR1/CCR2 only; AUTOCAL is intentionally off for now. */
#define AD9959_TIM4_MONITOR_ENABLE  1
#define AD9959_TIM4_AUTOCAL_ENABLE  0

/* Keep a DMA fault frozen for Ozone inspection.  Set to 1 only after the
 * fault cause is understood and automatic recovery is desired. */
#define AD9959_DMA_AUTO_RESTART  1

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
