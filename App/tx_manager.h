/**
 ******************************************************************************
 * @file    tx_manager.h
 * @brief   Transmission Manager — Application Layer
 *
 * Orchestrates modulation, encoding, buffering, and DMA triggering.
 ******************************************************************************
 */

#ifndef __TX_MANAGER_H__
#define __TX_MANAGER_H__

#include <stdint.h>
#include <stdbool.h>
#include "dds_command.h"

typedef enum {
    TX_MODE_CW  = 0,
    TX_MODE_FSK = 1,
    TX_MODE_ASK = 2,
    TX_MODE_IDLE = 0xFF,
} TX_Mode;

void     TX_Manager_Init(void);
void     TX_Manager_Start(void);
void     TX_Manager_Stop(void);
void     TX_Manager_SetMode(TX_Mode mode);

/**
 * @brief  Enqueue data bits for transmission
 * @param  data     : byte array of payload bits
 * @param  num_bits : number of bits
 */
void     TX_Manager_Send(const uint8_t *data, int num_bits);

bool     TX_Manager_IsBusy(void);

#endif /* __TX_MANAGER_H__ */
