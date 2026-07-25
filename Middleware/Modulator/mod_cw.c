/**
 ******************************************************************************
 * @file    mod_cw.c
 * @brief   CW Modulator Implementation
 ******************************************************************************
 */

#include "mod_cw.h"
#include "dds_encoder.h"

void CW_Generate(uint32_t ftw, uint16_t asf, uint8_t profile,
                 DDS_Command *cmd)
{
    cmd->ftw     = ftw;
    cmd->asf     = asf;
    cmd->pow     = 0;
    cmd->profile = profile;
    cmd->update_kind = DDS_UPDATE_FTW;
}

int CW_GenerateFrame(uint32_t ftw, uint16_t asf, uint8_t profile,
                     uint8_t *spi1_buf, uint8_t *spi3_buf)
{
    DDS_Command cmd;
    CW_Generate(ftw, asf, profile, &cmd);
    return Encoder_FormatCommand(&cmd, spi1_buf, spi3_buf);
}
