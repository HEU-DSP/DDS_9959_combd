/**
 ******************************************************************************
 * @file    symbol.h
 * @brief   Symbol Mapper — Skeleton (Phase 4)
 ******************************************************************************
 */

#ifndef __SYMBOL_H__
#define __SYMBOL_H__

#include <stdint.h>

typedef int16_t Symbol;   /* IQ symbol: I in high byte, Q in low byte */

int Symbol_FromBits(const uint8_t *bits, int num_bits,
                    Symbol *symbols, int max_syms, int bits_per_sym);

#endif /* __SYMBOL_H__ */
