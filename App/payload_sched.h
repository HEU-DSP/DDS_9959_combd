/**
 ******************************************************************************
 * @file    payload_sched.h
 * @brief   Payload scheduler — repeat shadow, queue promotion, idle policy
 *
 * Owns the App-side handoff semantics of the symbol queue:
 *   - PayloadSched_Stage():   SET_SYMBOLS entry — stages into the queue and
 *     records the payload as the repeat shadow (only on success).
 *   - PayloadSched_Task():    once per main-loop pass, both transport modes:
 *     ① promote a staged payload the moment the read slot drains
 *     ② refill the queue from the repeat shadow when repeat is on
 *     ③ optional bring-up auto pattern (AD9959_IDLE_AUTO_PATTERN)
 *   - PayloadSched_SetRepeat(): host SET_REPEAT (0x0C) — infinite replay of
 *     the most recent successfully staged payload.
 *   - PayloadSched_Reset():   APPLY / STOP_OUTPUT / debug-mode entry / boot —
 *     drops the queue, the shadow and the repeat flag, so a stopped
 *     transmitter can never restart itself.
 *
 * Idle policy (no payload, repeat off): the encoders emit the MARK symbol
 * (ENCODER_IDLE_SYMBOL in channel_config.h) — a fixed carrier instead of
 * the legacy 0/1 auto pattern.
 *
 * Never called from an ISR.  Main-loop context only.
 ******************************************************************************
 */

#ifndef __PAYLOAD_SCHED_H__
#define __PAYLOAD_SCHED_H__

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t  repeat_enabled;   /* SET_REPEAT state                        */
    uint8_t  repeat_valid;     /* shadow holds a payload                  */
    uint16_t repeat_count;     /* shadow bit count                        */
    uint32_t stage_ok;         /* successful stagings                     */
    uint32_t stage_busy;       /* BUSY rejections                         */
    uint32_t promote_count;    /* staged → playing promotions             */
    uint32_t refill_count;     /* repeat shadow refills                   */
} PayloadSchedDebug;

extern volatile PayloadSchedDebug payload_sched_debug;

void PayloadSched_Init(void);

/** @brief  Main-loop task: promote / repeat-refill / optional auto pattern. */
void PayloadSched_Task(void);

/**
 * @brief  Stage a payload (SET_SYMBOLS).  On success it becomes the new
 *         repeat shadow; on BUSY the shadow is left unchanged.
 * @return true when staged.
 */
bool PayloadSched_Stage(const uint8_t *bits, uint16_t count);

/** @brief  Host SET_REPEAT: enable/disable infinite replay. */
void PayloadSched_SetRepeat(bool on);

bool PayloadSched_RepeatEnabled(void);

bool PayloadSched_HasShadow(void);

/** @brief  Drop queue + shadow + repeat flag (APPLY/STOP/debug-entry/boot). */
void PayloadSched_Reset(void);

#endif /* __PAYLOAD_SCHED_H__ */
