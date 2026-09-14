/**
 ******************************************************************************
 * @file    tx_buffer.h
 * @brief   Transmit buffers, frame timing, and bank management
 *
 * Buffers:  static array of 2 banks × 2 SPI lanes
 * Timing:   sample_rate = baud_rate × oversampling (App layer decides factor)
 * Flow:     ISR fills freed bank via FrameBuilder, swaps active index
 ******************************************************************************
 */

#ifndef __TX_BUFFER_H__
#define __TX_BUFFER_H__

#include <stdint.h>

/* ================================================================
 * Buffer geometry
 * ================================================================ */

/* Max SPI frame bytes: 4-ch full update = 2+4×(5+3+4)=50, ceiling to 64.
 * Constrained by FRAME_MAX_BYTES (transfer time limit). */
#define FRAME_BUF_SIZE    64U       /* Bytes per SPI lane per bank */
#define TX_BANK_COUNT    2U         /* Ping + Pong                */

typedef struct {
    uint8_t spi1[FRAME_BUF_SIZE];   /* SPI1 MOSI data (SDIO0) */
    uint8_t spi3[FRAME_BUF_SIZE];   /* SPI3 MOSI data (SDIO1) */
} FrameBank;

/* ---- Static array: tx_bank[0] = ping, tx_bank[1] = pong ---- */
extern FrameBank  tx_bank[TX_BANK_COUNT];
extern volatile uint8_t  tx_active;   /* bank DMA is reading (0 or 1) */
extern volatile uint16_t tx_bank_bytes; /* bytes per bank — ISR-read, main-written */

/**
 * @brief  Return pointer to the idle bank (ready for CPU fill).
 */
static inline FrameBank *TxBuf_GetIdle(void)
{
    return &tx_bank[tx_active ^ 1];
}

/**
 * @brief  Return pointer to the active bank (DMA is sending this).
 */
static inline FrameBank *TxBuf_GetActive(void)
{
    return &tx_bank[tx_active];
}

/**
 * @brief  Toggle active bank (call from DMA ISR after re-arming).
 */
static inline void TxBuf_Swap(void)
{
    tx_active ^= 1;
}

/* ================================================================
 * Frame timing (derived from App-layer modulation config)
 * ================================================================ */

typedef struct {
    uint32_t baud_rate;        /* Symbol rate from App (symbols/sec)     */
    uint32_t samples_per_sym;  /* Oversampling factor (1=CW/FSK, 4=GFSK) */
    uint32_t sample_rate;      /* = baud_rate × samples_per_sym (Hz)     */
} TxTiming;

extern TxTiming  tx_timing;

/**
 * @brief  Compute sample_rate from baud_rate + oversampling.
 * @note   Call once after App sets baud_rate and samples_per_sym.
 *         The LPTIM3 ARR is computed separately in trigger.c via
 *         BSP_LPTIM3_ARRForRate(cfg->sample_rate).
 */
static inline void TxTiming_Update(void)
{
    tx_timing.sample_rate = tx_timing.baud_rate * tx_timing.samples_per_sym;
}

#endif /* __TX_BUFFER_H__ */
