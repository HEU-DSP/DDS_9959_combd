/**
 ******************************************************************************
 * @file    mod_4fsk.h
 * @brief   4-level Frequency Shift Keying (4FSK) Modulator
 ******************************************************************************
 */

#ifndef __MOD_4FSK_H__
#define __MOD_4FSK_H__

#include "dds_command.h"

typedef struct {
    uint32_t ftw[4];      /* symbol 0..3 frequency table */
    uint16_t asf;
    uint8_t  profile;
} FSK4_Config;

void FSK4_GenerateSymbol(const FSK4_Config *cfg, uint8_t symbol,
                         DDS_Command *cmd);
int FSK4_GenerateFrame(const FSK4_Config *cfg, uint8_t symbol,
                       uint8_t *spi1_buf, uint8_t *spi3_buf);

#endif /* __MOD_4FSK_H__ */
