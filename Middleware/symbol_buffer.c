#include "symbol_buffer.h"
#include <stddef.h>    /* NULL */

/* Two-slot ping-pong queue.
 *
 * sym_read  = the slot the consumer is draining (TIM8 ISR in downgrade
 *             mode, main loop in DMA mode)
 * sym_write = the slot the producer stages into (main loop only)
 *
 * INVARIANT: only producer-context functions (WriteBits / Promote / Clear)
 * ever change sym_read / sym_write, and only while the read slot is not
 * playing (!X_READY(read)).  The consumer (ReadBits) only releases its own
 * slot — never the indices.  No WriteBits / Promote / Clear may ever be
 * called from an ISR. */
static SymbolBuffer sym_slots[2];
static volatile uint8_t sym_read  = 0U;
static volatile uint8_t sym_write = 1U;

#define X_READY(s) ((sym_slots[(s)].ready) && ((sym_slots[(s)].count) > 0U))
#define X_FREE(s)  ((sym_slots[(s)].free))

static void slot_reset(SymbolBuffer *s)
{
    s->count = 0U;
    s->read_index = 0U;
    s->bits_per_sym = 1U;
    s->ready = false;
    s->free = true;
}

void SymbolBuf_Clear(void)
{
    slot_reset(&sym_slots[0]);
    slot_reset(&sym_slots[1]);
    sym_read  = 0U;
    sym_write = 1U;
}

bool SymbolBuf_WriteBits(const uint8_t *bits, uint16_t count)
{
    if ((bits == NULL) || (count == 0U) || (count > SYM_BUF_CAPACITY)) {
        return false;
    }

    /* Promote a fully staged payload first, preserving FIFO order. */
    (void)SymbolBuf_Promote();

    SymbolBuffer *w = &sym_slots[sym_write];
    if (!w->free) {
        return false;   /* playing + staged both occupied → BUSY */
    }

    for (uint16_t i = 0U; i < count; i++) {
        w->data[i] = bits[i] & 0x01U;   /* one bit per element, LSB */
    }
    w->count = count;
    w->read_index = 0U;
    w->bits_per_sym = 1U;
    w->ready = true;
    w->free = false;

    /* Nothing playing → make this slot the read slot immediately, so the
     * next ReadBits sees it without waiting for a Promote. */
    if (!X_READY(sym_read)) {
        uint8_t t = sym_read;
        sym_read  = sym_write;
        sym_write = t;
    }
    return true;
}

bool SymbolBuf_ReadBits(uint8_t *value, uint8_t nbits)
{
    if ((value == NULL) || (nbits == 0U) || (nbits > 8U)) {
        return false;
    }

    SymbolBuffer *r = &sym_slots[sym_read];
    if (!r->ready) {
        return false;
    }

    if (r->count < nbits) {
        /* Retire the unusable tail: a partial symbol can never complete,
         * and leaving it would wedge the queue (the producer would see a
         * busy read slot forever).  The caller falls back to the idle
         * symbol for this one frame. */
        slot_reset(r);
        return false;
    }

    uint8_t out = 0U;
    for (uint8_t i = 0U; i < nbits; i++) {
        out = (uint8_t)((out << 1) | (r->data[r->read_index] & 0x01U));
        r->read_index++;
        r->count--;
    }
    *value = out;

    if (r->count == 0U) {
        /* Release the slot — the handoff back to the producer, which
         * promotes any staged payload. */
        r->ready = false;
        r->free = true;
        r->read_index = 0U;
    }
    return true;
}

bool SymbolBuf_Promote(void)
{
    if (!X_READY(sym_read) && X_READY(sym_write)) {
        uint8_t t = sym_read;
        sym_read  = sym_write;
        sym_write = t;
        return true;
    }
    return false;
}

bool SymbolBuf_HasData(void)
{
    return X_READY(sym_read);
}

bool SymbolBuf_IsStaged(void)
{
    return X_READY(sym_write);
}

bool SymbolBuf_IsFree(void)
{
    /* "A WriteBits would succeed right now": write slot free, or the read
     * slot is drained and will become the write target after the Promote
     * that WriteBits performs first. */
    return X_FREE(sym_write) || (X_FREE(sym_read) && !X_READY(sym_read));
}

uint16_t SymbolBuf_Count(void)
{
    return sym_slots[sym_read].count;
}

/* ---- Ozone / debug accessors ---- */
const SymbolBuffer *SymbolBuf_GetReadSlot(void)
{
    return &sym_slots[sym_read];
}

const SymbolBuffer *SymbolBuf_GetWriteSlot(void)
{
    return &sym_slots[sym_write];
}

uint8_t SymbolBuf_ReadIndex(void)
{
    return sym_read;
}
