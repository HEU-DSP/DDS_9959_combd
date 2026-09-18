/**
 ******************************************************************************
 * @file    symbol_buffer.h
 * @brief   Two-slot bit queue between App and Middleware
 *
 * The queue is a ping-pong pair: one slot is being drained by the consumer
 * (Encoder in main loop, or the TIM8 ISR in downgrade mode), the other
 * accepts the next staged payload from the host protocol.  This gives the
 * host one payload of look-ahead (double-buffer queueing).
 *
 * CONCURRENCY CONTRACT:
 *   - WriteBits / Promote / Clear are MAIN-LOOP ONLY.  Never call them from
 *     an ISR.  They are the only functions that mutate the slot indices.
 *   - ReadBits (the consumer, possibly ISR context) only releases its own
 *     slot; it never touches the indices.
 *   - Callers that historically cleared the buffer while an ISR could be
 *     mid-read (APPLY/STOP/debug entry) must keep their ISR-quiesced
 *     discipline (mask TIM8->DIER, spin on ds.tim8_isr_active in downgrade
 *     mode; Trigger_Stop in DMA mode).
 *
 * Storage: one bit per element (each byte masked to &0x01), MSB-first on
 * multi-bit reads, so `bits_per_sym` means "bits per element" and stays 1.
 ******************************************************************************
 */

#ifndef __SYMBOL_BUFFER_H__
#define __SYMBOL_BUFFER_H__

#include <stdint.h>
#include <stdbool.h>

#define SYM_BUF_CAPACITY  256U

typedef struct {
    uint8_t   data[SYM_BUF_CAPACITY];
    uint16_t  count;          /* Remaining valid symbols                     */
    uint16_t  read_index;     /* Next symbol index for Middleware read       */
    uint8_t   bits_per_sym;   /* Bits per element (always 1 = bit-per-byte)  */
    volatile bool ready;      /* App -> Middleware: data available           */
    volatile bool free;       /* Middleware -> App: buffer writable          */
} SymbolBuffer;

/**
 * @brief  Reset both slots (main-loop only; quiesce any ISR reader first).
 */
void SymbolBuf_Clear(void);

/**
 * @brief  Stage a payload.  Promotes a fully staged slot first (FIFO).
 * @return true on success, false when both slots are occupied (BUSY).
 */
bool SymbolBuf_WriteBits(const uint8_t *bits, uint16_t count);

/**
 * @brief  Read nbits (1..8) MSB-first.  On a partial tail (count < nbits)
 *         the tail is retired and false returned — the caller falls back
 *         to the idle symbol.  Releases the slot when drained.
 */
bool SymbolBuf_ReadBits(uint8_t *value, uint8_t nbits);

/**
 * @brief  Make a staged payload the playing one (main-loop only).
 * @return true when a promotion happened.
 */
bool SymbolBuf_Promote(void);

/** @return true when the read slot has data. */
bool SymbolBuf_HasData(void);

/** @return true when a payload is staged in the write slot. */
bool SymbolBuf_IsStaged(void);

/** @return true when a WriteBits call would succeed right now. */
bool SymbolBuf_IsFree(void);

/** @return remaining bits in the read slot. */
uint16_t SymbolBuf_Count(void);

/* ---- Ozone / debug accessors (read-only views) ---- */
const SymbolBuffer *SymbolBuf_GetReadSlot(void);
const SymbolBuffer *SymbolBuf_GetWriteSlot(void);
uint8_t             SymbolBuf_ReadIndex(void);

#endif /* __SYMBOL_BUFFER_H__ */
