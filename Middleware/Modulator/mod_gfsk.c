/**
 ******************************************************************************
 * @file    mod_gfsk.c
 * @brief   GFSK Modulator Implementation
 ******************************************************************************
 */

#include "mod_gfsk.h"
#include "dds_encoder.h"

static uint32_t add_ftw_offset(uint32_t center, int32_t offset)
{
    return (offset >= 0) ? (center + (uint32_t)offset)
                         : (center - (uint32_t)(-offset));
}

void GFSK_GenerateBit(const GFSK_Config *cfg, uint8_t bit, DDS_Command *cmd)
{
    int32_t target = bit ? 256 : -256;
    int32_t state = (cfg->filter_state != 0) ? *cfg->filter_state : target;

    /* Lightweight first-order Gaussian-like smoothing for Phase-1 validation.
     * Replace with FIR Gaussian pulse shaping when oversampling is added. */
    state += (target - state) / 4;
    if (cfg->filter_state != 0) {
        *cfg->filter_state = state;
    }

    int32_t offset = (int32_t)(((int64_t)cfg->deviation_ftw * state) / 256);
    cmd->ftw = add_ftw_offset(cfg->center_ftw, offset);
    cmd->asf = cfg->asf;
    cmd->pow = 0;
    cmd->profile = cfg->profile;
}

int GFSK_GenerateFrame(const GFSK_Config *cfg, uint8_t bit,
                       uint8_t *spi1_buf, uint8_t *spi3_buf)
{
    DDS_Command cmd;
    GFSK_GenerateBit(cfg, bit, &cmd);
    return Encoder_FormatCommand(&cmd, spi1_buf, spi3_buf);
}
