/**
 ******************************************************************************
 * @file    symbol_buffer.h
 * @brief   Raw bit/symbol buffer between App and Middleware
 *
 * App writes a bit sequence. Middleware consumes one bit at a time while
 * refilling the TX ping-pong bank.
 ******************************************************************************
 */

#ifndef __SYMBOL_BUFFER_H__
#define __SYMBOL_BUFFER_H__

#include <stdint.h>
#include <stdbool.h>

#define SYM_BUF_CAPACITY  256U

typedef struct {
    uint8_t   data[SYM_BUF_CAPACITY];
    uint16_t  count;          /* Remaining valid symbols */
    uint16_t  read_index;     /* Next symbol index for Middleware read */
    uint16_t  write_index;    /* Next symbol index for App write */
    uint8_t   bits_per_sym;   /* Bits per symbol (1 for FSK/ASK, 0 for CW) */
    volatile bool ready;      /* App -> Middleware: data available */
    volatile bool free;       /* Middleware -> App: buffer writable */
} SymbolBuffer;

extern SymbolBuffer sym_buf;

void SymbolBuf_Clear(void);
bool SymbolBuf_WriteBits(const uint8_t *bits, uint16_t count);
bool SymbolBuf_ReadBit(uint8_t *bit);
bool SymbolBuf_HasData(void);
bool SymbolBuf_IsFree(void);
uint16_t SymbolBuf_Count(void);

#endif /* __SYMBOL_BUFFER_H__ */
