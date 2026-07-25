/**
 ******************************************************************************
 * @file    mod_am.c
 * @brief   AM Modulator Implementation
 ******************************************************************************
 */

#include "mod_am.h"
#include "dds_encoder.h"

static uint16_t clamp_asf(int32_t value)
{
    if (value < 0) {
        return 0U;
    }
    if (value > 0x3FF) {
        return 0x3FFU;
    }
    return (uint16_t)value;
}

void AM_GenerateSample(const AM_Config *cfg, int16_t sample, DDS_Command *cmd)
{
    int32_t delta = ((int32_t)cfg->asf_delta * sample) / 32767;
    cmd->ftw = cfg->ftw;
    cmd->asf = clamp_asf((int32_t)cfg->asf_center + delta);
    cmd->pow = 0;
    cmd->profile = cfg->profile;
    cmd->update_kind = DDS_UPDATE_ASF;
}

int AM_GenerateFrame(const AM_Config *cfg, int16_t sample,
                     uint8_t *spi1_buf, uint8_t *spi3_buf)
{
    DDS_Command cmd;
    AM_GenerateSample(cfg, sample, &cmd);
    return Encoder_FormatCommand(&cmd, spi1_buf, spi3_buf);
}
