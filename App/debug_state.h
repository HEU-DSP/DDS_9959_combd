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
    /* ---- Counters ---- */
    uint32_t frame_count;        /* 总发送帧数 */
    uint32_t dma_error_count;    /* DMA 错误次数 (DMAMUX overrun) */

    /* ---- DMA mode handshake (ISR → main loop) ---- */
    uint8_t  dma_bank_ready;     /* 1 = idle bank refill requested by DMA TC */

    /* ---- Downgrade-mode ISR handshake ---- */
    volatile uint8_t tim8_isr_active; /* 1 while TIM8 ISR is mid-transfer */
} DebugState;

/* Ozone-only observability for the AD9959 bring-up path.  Recording these
 * values must never alter the DMA, SPI or timer timing chain. */
typedef struct {
    uint32_t init_stage;

    uint32_t spi1_start_count;
    uint32_t spi1_done_count;
    uint32_t spi1_error;
    uint32_t spi1_start_status;
    uint32_t spi1_hal_state;
    uint32_t dma1_lisr;
    uint32_t dma1_hisr;
    uint32_t dmamux1_csr;
    uint32_t spi1_buffer_addr;

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
