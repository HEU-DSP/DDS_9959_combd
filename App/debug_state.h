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

extern DebugState ds;

#endif /* __DEBUG_STATE_H__ */
