/**
 ******************************************************************************
 * @file    symbol_buffer.h
 * @brief   Raw symbol buffer between App and Middleware
 *
 * App writes raw bitstream; Middleware reads and encodes into SPI frames.
 ******************************************************************************
 */

#ifndef __SYMBOL_BUFFER_H__
#define __SYMBOL_BUFFER_H__

#include <stdint.h>
#include <stdbool.h>

#define SYM_BUF_CAPACITY  256

typedef struct {
    uint8_t   data[SYM_BUF_CAPACITY];
    uint16_t  count;           /* Number of valid symbols */
    uint8_t   bits_per_sym;    /* Bits per symbol (1 for FSK/ASK, 0 for CW) */
    volatile bool ready;       /* App → Middleware: data available */
    volatile bool free;        /* Middleware → App: buffer writable */
} SymbolBuffer;

extern SymbolBuffer sym_buf;

#endif /* __SYMBOL_BUFFER_H__ */
