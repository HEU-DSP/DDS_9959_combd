/**
 ******************************************************************************
 * @file    mod_4fsk.c
 * @brief   4FSK Modulator Implementation
 ******************************************************************************
 */

#include "mod_4fsk.h"
#include "dds_encoder.h"

void FSK4_GenerateSymbol(const FSK4_Config *cfg, uint8_t symbol,
                         DDS_Command *cmd)
{
    uint8_t index = symbol & 0x03U;

    cmd->ftw = cfg->ftw[index];
    cmd->asf = cfg->asf;
    cmd->pow = 0;
    cmd->profile = cfg->profile;
}

int FSK4_GenerateFrame(const FSK4_Config *cfg, uint8_t symbol,
                       uint8_t *spi1_buf, uint8_t *spi3_buf)
{
    DDS_Command cmd;
    FSK4_GenerateSymbol(cfg, symbol, &cmd);
    return Encoder_FormatCommand(&cmd, spi1_buf, spi3_buf);
}
