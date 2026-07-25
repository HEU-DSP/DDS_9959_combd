/**
 ******************************************************************************
 * @file    mod_fm.c
 * @brief   FM Modulator Implementation
 ******************************************************************************
 */

#include "mod_fm.h"
#include "dds_encoder.h"

static uint32_t add_ftw_offset(uint32_t center, int32_t offset)
{
    return (offset >= 0) ? (center + (uint32_t)offset)
                         : (center - (uint32_t)(-offset));
}

void FM_GenerateSample(const FM_Config *cfg, int16_t sample, DDS_Command *cmd)
{
    int32_t offset = (int32_t)(((int64_t)cfg->deviation_ftw * sample) / 32767);
    cmd->ftw = add_ftw_offset(cfg->center_ftw, offset);
    cmd->asf = cfg->asf;
    cmd->pow = 0;
    cmd->profile = cfg->profile;
    cmd->update_kind = DDS_UPDATE_FTW;
}

int FM_GenerateFrame(const FM_Config *cfg, int16_t sample,
                     uint8_t *spi1_buf, uint8_t *spi3_buf)
{
    DDS_Command cmd;
    FM_GenerateSample(cfg, sample, &cmd);
    return Encoder_FormatCommand(&cmd, spi1_buf, spi3_buf);
}
