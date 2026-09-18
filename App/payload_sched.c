/**
 ******************************************************************************
 * @file    payload_sched.c
 * @brief   Payload scheduler — see payload_sched.h
 ******************************************************************************
 */

#include "payload_sched.h"
#include "symbol_buffer.h"
#include "phase1_config.h"      /* AD9959_IDLE_AUTO_PATTERN   */
#if AD9959_IDLE_AUTO_PATTERN
#include "channel_config.h"     /* Encoder_InputBits          */
#include "symbol_source.h"      /* bring-up auto pattern only */
#endif

volatile PayloadSchedDebug payload_sched_debug = {0};

/* Repeat shadow: a copy of the most recent successfully staged payload.
 * 256 bits = 32 bytes, one bit per element — same convention as the queue. */
/* The repeat state lives IN payload_sched_debug (Ozone-visible, volatile) —
 * repeat_* fields are the canonical storage, not a mirror. */
static uint8_t repeat_shadow[SYM_BUF_CAPACITY];

void PayloadSched_Init(void)
{
    PayloadSched_Reset();
    payload_sched_debug = (PayloadSchedDebug){0};
}

void PayloadSched_Reset(void)
{
    SymbolBuf_Clear();
    payload_sched_debug.repeat_count = 0U;
    payload_sched_debug.repeat_enabled = 0U;
    payload_sched_debug.repeat_valid = 0U;
}

bool PayloadSched_Stage(const uint8_t *bits, uint16_t count)
{
    if (!SymbolBuf_WriteBits(bits, count)) {
        payload_sched_debug.stage_busy++;
        return false;
    }

    /* Only an accepted payload becomes the repeat target. */
    for (uint16_t i = 0U; i < count; i++) {
        repeat_shadow[i] = bits[i] & 0x01U;
    }
    payload_sched_debug.repeat_count = count;
    payload_sched_debug.repeat_valid = 1U;
    payload_sched_debug.stage_ok++;
    return true;
}

void PayloadSched_SetRepeat(bool on)
{
    payload_sched_debug.repeat_enabled = on ? 1U : 0U;
}

bool PayloadSched_RepeatEnabled(void)
{
    return payload_sched_debug.repeat_enabled != 0U;
}

bool PayloadSched_HasShadow(void)
{
    return payload_sched_debug.repeat_valid != 0U;
}

void PayloadSched_Task(void)
{
    /* 1. Promote a staged payload the moment the read slot drains — the
     *    handoff is gapless in DMA mode (drain happens in the bank build,
     *    promote runs on the next pass before the next build). */
    if (!SymbolBuf_HasData() && SymbolBuf_Promote()) {
        payload_sched_debug.promote_count++;
    }

    /* 2. Repeat refill: a staged payload always wins (it occupies the
     *    write slot, so WriteBits would reject the refill anyway). */
    if (!SymbolBuf_HasData() && !SymbolBuf_IsStaged()
        && (payload_sched_debug.repeat_valid != 0U)
        && (payload_sched_debug.repeat_enabled != 0U)) {
        if (SymbolBuf_WriteBits(repeat_shadow,
                                payload_sched_debug.repeat_count)) {
            payload_sched_debug.refill_count++;
        }
    }

    /* 3. Optional bring-up auto pattern (0/1 alternating) — main-loop
     *    only, never from an ISR. */
#if AD9959_IDLE_AUTO_PATTERN
    if (!SymbolBuf_HasData() && !SymbolBuf_IsStaged() && SymbolBuf_IsFree()) {
        uint8_t ib = Encoder_InputBits();
        if (ib > 0U) {
            SymbolSource_Generate(ib);
        }
    }
#endif
}
