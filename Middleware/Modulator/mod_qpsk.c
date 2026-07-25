/**
 ******************************************************************************
 * @file    mod_qpsk.c
 * @brief   QPSK Modulator Implementation
 ******************************************************************************
 */

#include "mod_qpsk.h"
#include "dds_encoder.h"

void QPSK_GenerateSymbol(const QPSK_Config *cfg, uint8_t symbol,
                         DDS_Command *cmd)
{
    cmd->ftw = cfg->ftw;
    cmd->asf = cfg->asf;
    cmd->profile = cfg->profile;
    cmd->update_kind = DDS_UPDATE_POW;

    switch (symbol & 0x03U) {
    case 0x00:
        cmd->pow = cfg->phase_00;
        break;
    case 0x01:
        cmd->pow = cfg->phase_01;
        break;
    case 0x02:
        cmd->pow = cfg->phase_10;
        break;
    default:
        cmd->pow = cfg->phase_11;
        break;
    }
}

int QPSK_GenerateFrame(const QPSK_Config *cfg, uint8_t symbol,
                       uint8_t *spi1_buf, uint8_t *spi3_buf)
{
    DDS_Command cmd;
    QPSK_GenerateSymbol(cfg, symbol, &cmd);
    return Encoder_FormatCommand(&cmd, spi1_buf, spi3_buf);
}
