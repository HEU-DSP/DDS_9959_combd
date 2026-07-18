/**
 ******************************************************************************
 * @file    tx_buffer.c
 * @brief   Static storage for transmit buffers and timing
 ******************************************************************************
 */

#include "tx_buffer.h"

/* ---- Two-bank frame buffer array ---- */
FrameBank  tx_bank[TX_BANK_COUNT];

/* ---- Flow control ---- */
volatile uint8_t  tx_active    = 0;
uint16_t          tx_bank_bytes = 0;

/* ---- Frame timing ---- */
TxTiming  tx_timing;
