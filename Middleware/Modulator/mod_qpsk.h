/**
 ******************************************************************************
 * @file    mod_qpsk.h
 * @brief   Quadrature Phase Shift Keying (QPSK) Modulator
 ******************************************************************************
 */

#ifndef __MOD_QPSK_H__
#define __MOD_QPSK_H__

#include "dds_command.h"

typedef struct {
    uint32_t ftw;
    uint16_t asf;
    uint16_t phase_00;
    uint16_t phase_01;
    uint16_t phase_10;
    uint16_t phase_11;
    uint8_t  profile;
} QPSK_Config;

void QPSK_GenerateSymbol(const QPSK_Config *cfg, uint8_t symbol,
                         DDS_Command *cmd);
int QPSK_GenerateFrame(const QPSK_Config *cfg, uint8_t symbol,
                       uint8_t *spi1_buf, uint8_t *spi3_buf);

#endif /* __MOD_QPSK_H__ */
