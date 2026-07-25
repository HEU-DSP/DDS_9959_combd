/**
 ******************************************************************************
 * @file    debug_state.h
 * @brief   Debug counters and timing captures
 *
 * Read-only from main loop for debugging; written by DMA ISR.
 * Buffers live in tx_buffer.h.  Timing config lives in tx_buffer.h.
 ******************************************************************************
 */

#ifndef __DEBUG_STATE_H__
#define __DEBUG_STATE_H__

#include <stdint.h>

typedef struct {
    /* ---- Modulation test state ---- */
    int      mod_test_bit;
    int      pattern;            /* 0=CW, 1=FSK, 2=ASK */

    /* ---- TIM4 captures (CS↓ / CS↑ timing) ---- */
    uint32_t tim4_cs_start;      /* 当前帧 CS↓ 时刻 (TIM4 ticks) */
    uint32_t tim4_cs_end;        /* 当前帧 CS↑ 时刻             */
    uint32_t tim4_last_start;    /* 上一帧 CS↓ 时刻             */
    uint32_t tim4_last_end;      /* 上一帧 CS↑ 时刻             */

    /* ---- Counters ---- */
    uint32_t frame_count;        /* 总发送帧数 */
    uint32_t dma_error_count;    /* DMA 错误次数 */
} DebugState;

/* Ozone-only observability for the AD9959 bring-up path.  Recording these
 * values must never alter the DMA, SPI or timer timing chain. */
typedef struct {
    uint32_t init_stage;
    uint32_t master_reset;
    uint32_t power_down;

    uint32_t spi1_start_count;
    uint32_t spi3_start_count;
    uint32_t spi1_done_count;
    uint32_t spi3_done_count;
    uint32_t spi1_error;
    uint32_t spi3_error;
    uint32_t spi1_start_status;
    uint32_t spi3_start_status;
    uint32_t spi1_hal_state;
    uint32_t spi3_hal_state;
    uint32_t spi1_dma_error;
    uint32_t spi3_dma_error;
    uint32_t dma1_lisr;
    uint32_t dma1_hisr;
    uint32_t dmamux1_csr;
    uint32_t spi1_buffer_addr;
    uint32_t spi3_buffer_addr;

    uint32_t dma_frame_bytes;
    uint32_t frame_count;
    uint32_t tx_running;
    uint32_t tx_stop_pending;
    uint32_t tim8_ccr1;
    uint32_t tim8_ccr2;
} AD9959_Diag;

extern DebugState ds;
extern volatile AD9959_Diag ad9959_diag;

#endif /* __DEBUG_STATE_H__ */
