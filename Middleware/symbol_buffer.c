#include "symbol_buffer.h"

SymbolBuffer sym_buf = { .bits_per_sym = 1U, .free = true };

void SymbolBuf_Clear(void)
{
    sym_buf.count = 0;
    sym_buf.read_index = 0;
    sym_buf.write_index = 0;
    sym_buf.bits_per_sym = 1U;
    sym_buf.ready = false;
    sym_buf.free = true;
}

bool SymbolBuf_WriteBits(const uint8_t *bits, uint16_t count)
{
    if ((bits == 0) || (count == 0U) || (count > SYM_BUF_CAPACITY)) {
        return false;
    }

    /* The buffer is single-producer/single-consumer: App may only replace
     * data after Middleware has consumed the previous sequence. */
    if (!sym_buf.free) {
        return false;
    }

    for (uint16_t i = 0; i < count; i++) {
        sym_buf.data[i] = bits[i] & 0x01U;
    }

    sym_buf.count = count;
    sym_buf.read_index = 0;
    sym_buf.write_index = count;
    sym_buf.bits_per_sym = 1U;
    sym_buf.ready = true;
    sym_buf.free = false;
    return true;
}

bool SymbolBuf_ReadBit(uint8_t *bit)
{
    return SymbolBuf_ReadBits(bit, 1U);
}

bool SymbolBuf_ReadBits(uint8_t *value, uint8_t nbits)
{
    if ((value == 0) || !sym_buf.ready || (nbits == 0U) ||
        (nbits > 8U) || (sym_buf.count < nbits)) {
        return false;
    }

    uint8_t out = 0;
    for (uint8_t i = 0; i < nbits; i++) {
        out = (uint8_t)((out << 1) | (sym_buf.data[sym_buf.read_index] & 0x01U));
        sym_buf.read_index++;
        sym_buf.count--;
    }
    *value = out;

    if (sym_buf.count == 0U) {
        /* Releasing the buffer here is the handoff back to App/main context. */
        sym_buf.ready = false;
        sym_buf.free = true;
        sym_buf.read_index = 0;
        sym_buf.write_index = 0;
    }

    return true;
}

bool SymbolBuf_HasData(void)
{
    return sym_buf.ready && (sym_buf.count > 0U);
}

bool SymbolBuf_IsFree(void)
{
    return sym_buf.free;
}

uint16_t SymbolBuf_Count(void)
{
    return sym_buf.count;
}
