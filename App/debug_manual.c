/**
 ******************************************************************************
 * @file    debug_manual.c
 * @brief   Manual debug mode implementation — see debug_manual.h
 ******************************************************************************
 */

#include "debug_manual.h"
#include "dds_calc.h"
#include "symbol_buffer.h"
#include "main.h"           /* HAL_GetTick */
#include "trigger.h"        /* Trigger_Stop / Trigger_Restart (chain bracket) */
#include "ad9959.h"         /* AD9959_ReassertPLL + PC6 borrow/restore        */
#include "dds_encoder.h"    /* Encoder_WriteStaticRegs (CSR + CFR)            */
#include "debug_state.h"    /* ad9959_diag.tx_running                          */

/* ---- Master switch ---- */
volatile uint32_t debug_manual_enable = 0U;

/* Function-transition counter (OFF↔CW↔SWEEP), Ozone-visible.  Every change
 * detected by DebugManual_Task() also re-asserts the boot-once registers
 * (FR1/CFR), so this counts those heal cycles as well. */
volatile uint32_t debug_transition_count = 0U;

/* Defaults mirror the reference project's sweep UI (Template1: 1 kHz → 10 kHz,
 * 100 Hz step, 1 ms dwell), so flipping a channel to DEBUG_CH_SWEEP sweeps
 * straight away. freq_hz is the shared CW/sweep-start field, so it takes the
 * sweep start value — a CW channel needs its frequency typed in regardless.
 * ASF starts at 0 so a bare enable cannot key the transmitter. */
#define DEBUG_CH_DEFAULT {                            \
    .function = DEBUG_CH_OFF, .freq_hz = 1000U,       \
    .sweep_max_hz = 10000U, .sweep_step_hz = 100U,    \
    .sweep_dwell_ms = 1U, .asf = 0U, .pow = 0U,       \
    .now_hz = 0U, .last_step_tick = 0U }

DebugChCfg debug_ch_cfg[DDS_CHANNEL_COUNT] = {
    DEBUG_CH_DEFAULT, DEBUG_CH_DEFAULT, DEBUG_CH_DEFAULT, DEBUG_CH_DEFAULT
};

/**
 * @brief  Advance one channel's sawtooth sweep to the current tick.
 *
 * Unsigned tick subtraction makes the 49.7-day HAL_GetTick wrap harmless as
 * long as the interval stays below 2^31 ms, which a <=255 ms dwell always is.
 *
 * @param  c         channel config (now_hz / last_step_tick updated in place)
 * @param  now_tick  current HAL_GetTick() snapshot
 * @param  start     clamped sweep start frequency
 * @param  stop      clamped sweep stop frequency (>= start)
 */
static void sweep_step(DebugChCfg *c, uint32_t now_tick,
                       uint32_t start, uint32_t stop)
{
    /* Re-seed whenever the debugger edits a limit out from under the sweep. */
    if ((c->now_hz < start) || (c->now_hz > stop)) {
        c->now_hz = start;
        c->last_step_tick = now_tick;
        return;
    }

    if (c->sweep_step_hz == 0U) {
        return;                         /* step 0 pins the channel at start */
    }
    if ((uint32_t)(now_tick - c->last_step_tick) < (uint32_t)c->sweep_dwell_ms) {
        return;
    }

    c->last_step_tick = now_tick;

    /* Both operands are clamped to DEBUG_FREQ_MAX_HZ, so this cannot wrap. */
    uint32_t next = c->now_hz + c->sweep_step_hz;
    c->now_hz = (next > stop) ? start : next;
}

/**
 * @brief  Re-assert the registers the 10 kHz DMA frame never rewrites.
 *
 * A master_reset glitch resets the chip: FR1 → 0 (PLL off, SYSCLK drops to
 * REF_CLK — the "reset-like" supply current drop) and CFR → 0 (DAC
 * full-scale current control bits [9:8] = 00, output current collapses).
 * The DMA frame refreshes CSR/CFTW/ACR/CPOW every 100 µs, but FR1 and CFR
 * are written only at boot/APPLY — so the damage persists until this runs.
 * Every function transition calls this, making manual mode self-healing.
 *
 * PC6 is borrowed as GPIO for the latch and handed back afterwards.
 * The caller must have the chain stopped (SPI1 idle) — either it was
 * already stopped, or Trigger_Stop() preceded the call; it must also
 * restart the chain afterwards.
 */
static void debug_heal_static_regs(void)
{
    uint8_t mask = 0U;
    for (uint8_t ch = 0U; ch < DDS_CHANNEL_COUNT; ch++) {
        if (mod_cfg[ch].enabled) {
            mask |= (uint8_t)(1U << ch);
        }
    }

    AD9959_IOUpdateGpioInit();        /* borrow PC6 as GPIO */
    Encoder_WriteStaticRegs(mask);    /* CSR + CFR (DAC full-scale bits) */
    AD9959_ReassertPLL();             /* FR1 (PLL x20) */
    AD9959_IOUpdate();                /* latch all three */
    HAL_Delay(1);                     /* PLL lock if it was dead (typ. 1 ms) */
    AD9959_IOUpdateTimerInit();       /* hand PC6 back to TIM8_CH1 */
}

void DebugManual_Task(void)
{
    static uint32_t prev_enable = 0U;
    static uint8_t  prev_function[DDS_CHANNEL_COUNT] = {0U};

    if (debug_manual_enable == 0U) {
        prev_enable = 0U;
        return;
    }

    uint32_t rising = (prev_enable == 0U) ? 1U : 0U;
    if (rising) {
        /* Rising edge: drop whatever symbols the host staged, so they cannot
         * suddenly replay when debug mode is switched off again. */
        SymbolBuf_Clear();
        prev_enable = 1U;
    }

    uint32_t now_tick = HAL_GetTick();

    /* Detect per-channel function transitions (OFF↔CW↔SWEEP).  The rising
     * edge counts as a transition on every channel, so entering debug mode
     * heals the boot-once registers once. */
    uint8_t fn_changed[DDS_CHANNEL_COUNT];
    bool    any_changed = false;
    for (uint8_t ch = 0U; ch < DDS_CHANNEL_COUNT; ch++) {
        uint8_t fn = (uint8_t)debug_ch_cfg[ch].function;
        fn_changed[ch] = (uint8_t)((rising || (prev_function[ch] != fn)) ? 1U : 0U);
        prev_function[ch] = fn;
        any_changed = any_changed || (fn_changed[ch] != 0U);
    }

    if (any_changed) {
        debug_transition_count++;

        /* Heal FR1/CFR on a frame boundary: stop the chain, re-assert, then
         * restart it.  If the chain is already stopped (STOP_OUTPUT, or a
         * dead chain), heal in place and leave it stopped — a transition
         * must never restart a deliberately stopped transmitter. */
        if (ad9959_diag.tx_running != 0U) {
            if (Trigger_Stop() == HAL_OK) {
                debug_heal_static_regs();
                ad9959_diag.tx_running = (uint32_t)(Trigger_Restart() == HAL_OK);
            } else {
                ad9959_diag.tx_running = 0U;  /* SPI never settled: chain dead */
            }
        } else {
            debug_heal_static_regs();
        }
    }

    for (uint8_t ch = 0U; ch < DDS_CHANNEL_COUNT; ch++) {
        DebugChCfg *c = &debug_ch_cfg[ch];

        /* Clamp to what the hardware and DDSCalc_FTW accept. */
        uint32_t start = (c->freq_hz      > DEBUG_FREQ_MAX_HZ) ? DEBUG_FREQ_MAX_HZ : c->freq_hz;
        uint32_t stop  = (c->sweep_max_hz > DEBUG_FREQ_MAX_HZ) ? DEBUG_FREQ_MAX_HZ : c->sweep_max_hz;
        uint16_t asf   = (c->asf > DDSCALC_ASF_MAX) ? (uint16_t)DDSCALC_ASF_MAX : c->asf;
        uint16_t pow   = (uint16_t)(c->pow & DEBUG_POW_MASK);
        uint32_t freq  = start;

        switch (c->function) {
        case DEBUG_CH_SWEEP:
            if (stop < start) {
                stop = start;           /* reference UI: min > max collapses to min */
            }
            if (fn_changed[ch] != 0U) {
                /* Entering SWEEP always restarts from freq_hz (the sweep
                 * start) instead of resuming from a stale now_hz left over
                 * from a previous sweep session. */
                c->now_hz = start;
                c->last_step_tick = now_tick;
            } else {
                sweep_step(c, now_tick, start, stop);
            }
            freq = c->now_hz;
            break;

        case DEBUG_CH_CW:
            break;

        case DEBUG_CH_OFF:
        default:
            /* Keep the channel in the frame as a silent carrier instead of
             * disabling it. A disabled channel makes Encoder_BuildBank_DMA()
             * return 0, which stops the DMA re-arm in HAL_SPI_TxCpltCallback
             * and with it every future DebugManual_Task() call — and APPLY,
             * the other way out, is refused while this mode is on. */
            asf = 0U;
            pow = 0U;
            break;
        }

        ModCfg_SetCW(ch, DDSCalc_FTW(freq), asf);
        mod_cfg[ch].cw.pow = pow;       /* setter clears pow, so write it after */
    }
}
