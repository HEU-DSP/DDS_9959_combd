/**
 ******************************************************************************
 * @file    symbol_source.h
 * @brief   Local auto symbol generator — replaces the hardcoded fsk_bit.
 *
 * When no host data is available in sym_buf, the ISR calls
 * SymbolSource_Generate() which writes test patterns appropriate
 * for each active channel's modulation mode.
 *
 * Host-supplied symbols (via SET_SYMBOLS) always take priority.
 ******************************************************************************
 */

#ifndef __SYMBOL_SOURCE_H__
#define __SYMBOL_SOURCE_H__

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief  Generate local test symbols into sym_buf.
 *
 * Must only be called when sym_buf is free (SymbolBuf_IsFree()).
 * Writes up to SYM_BUF_CAPACITY bits into the buffer.
 *
 * @param  input_bits   number of bits needed per ISR call
 *                      (from enabled_input_bits())
 */
void SymbolSource_Generate(uint8_t input_bits);

#endif /* __SYMBOL_SOURCE_H__ */
