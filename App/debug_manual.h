/**
 ******************************************************************************
 * @file    debug_manual.h
 * @brief   Manual debug mode — Ozone-driven per-channel CW / sweep generator
 *
 * Enabled at run time by setting `debug_manual_enable` to 1 from the
 * debugger (Ozone); no rebuild required.  While enabled, DebugManual_Task()
 * recomputes mod_cfg[] from debug_ch_cfg[] once per main-loop pass, so the
 * values travel the exact same hardware pipeline as the host protocol
 * (Encoder_BuildBank_DMA → TxBuf → LPTIM3/DMAMUX/SPI1/TIM8), and
 * APPLY / STOP_OUTPUT are refused with HOST_NACK_LOCAL_MODE.
 *
 * FUNCTION TRANSITIONS: switching a channel between OFF/CW/SWEEP while the
 * mode is on is detected every pass.  On any transition (and on the
 * enable rising edge) DebugManual_Task():
 *   - re-asserts the boot-once registers the DMA frame never rewrites —
 *     FR1 (PLL x20) and CFR (DAC full-scale current bits) — on a frame
 *     boundary (chain stop → write → latch → restart, or in place when the
 *     chain is already stopped).  A master_reset glitch therefore heals at
 *     the next transition instead of persisting until reboot;
 *   - re-seeds the sweep state, so entering SWEEP always restarts from
 *     freq_hz instead of resuming from a stale now_hz;
 *   - increments debug_transition_count (Ozone watch).
 *
 * Scope is the reference project's basic functions (DOCS/Template1):
 *   CW    — frequency + amplitude (ASF) + phase (CPOW)
 *   SWEEP — software linear sweep, sawtooth wrap; the CPU recomputes the
 *           FTW every dwell, matching the reference demo's TIM4-interrupt
 *           sweep (not the LSRR/RDW hardware-ramp registers)
 *
 * Usage (Ozone):
 *   1. fill debug_ch_cfg[0..3]: function / freq_hz / sweep_* / asf / pow
 *   2. set debug_manual_enable = 1
 *   3. watch mod_cfg[0..3].cw.{ftw,asf,pow} and debug_ch_cfg[ch].now_hz
 *   4. set debug_manual_enable = 0 to hand mod_cfg[] back to the host
 ******************************************************************************
 */

#ifndef __DEBUG_MANUAL_H__
#define __DEBUG_MANUAL_H__

#include <stdint.h>
#include "mod_config.h"     /* DDS_CHANNEL_COUNT */

/* ---- Limits (mirror DOCS/Template1 task_manage.c) ---- */
#define DEBUG_FREQ_MAX_HZ    200000000U   /* reference UI caps frequency at 200 MHz */
#define DEBUG_DWELL_MAX_MS   255U         /* reference SweepTime range 0..255      */
#define DEBUG_POW_MASK       0x3FFFU      /* 14-bit CPOW                          */

typedef enum {
    DEBUG_CH_OFF   = 0,
    DEBUG_CH_CW    = 1,
    DEBUG_CH_SWEEP = 2,
} DebugChFunction;

typedef struct {
    uint32_t function;        /* DebugChFunction — u32 so Ozone can write an int  */
    uint32_t freq_hz;         /* CW frequency, or sweep start frequency           */
    uint32_t sweep_max_hz;    /* sweep stop frequency; the sawtooth wraps here    */
    uint32_t sweep_step_hz;   /* Hz per step; 0 pins the channel at freq_hz       */
    uint16_t sweep_dwell_ms;  /* dwell per step; 0 steps once per frame           */
    uint16_t asf;             /* amplitude scale factor, 0..1023                  */
    uint16_t pow;             /* phase offset, 0..16383                           */

    /* ---- Runtime state: written back in place by DebugManual_Task() ---- */
    uint32_t now_hz;          /* current sweep frequency                          */
    uint32_t last_step_tick;  /* HAL_GetTick() snapshot of the last step          */
} DebugChCfg;

/* ---- Master switch: 0 = host protocol owns mod_cfg, 1 = debug mode ---- */
extern volatile uint32_t debug_manual_enable;

/* Function-transition counter — see the FUNCTION TRANSITIONS note above. */
extern volatile uint32_t debug_transition_count;

extern DebugChCfg debug_ch_cfg[DDS_CHANNEL_COUNT];

/**
 * @brief  Recompute mod_cfg[] from debug_ch_cfg[] and advance any sweep.
 * @note   Call once per DMA frame, immediately before Encoder_BuildBank_DMA().
 *         No-op while debug_manual_enable is 0.
 */
void DebugManual_Task(void);

#endif /* __DEBUG_MANUAL_H__ */
