/**
 ******************************************************************************
 * @file    mod_msk.c
 * @brief   MSK Modulator Implementation
 ******************************************************************************
 */

#include "mod_msk.h"
#include "dds_encoder.h"

static uint32_t add_ftw_offset(uint32_t center, int32_t offset)
{
    return (offset >= 0) ? (center + (uint32_t)offset)
                         : (center - (uint32_t)(-offset));
}

void MSK_GenerateBit(const MSK_Config *cfg, uint8_t bit, DDS_Command *cmd)
{
    uint16_t phase = (cfg->phase_acc != 0) ? *cfg->phase_acc : 0U;
    uint16_t step = cfg->phase_step & 0x3FFFU;
    int32_t offset = bit ? cfg->deviation_ftw : -cfg->deviation_ftw;

    phase = bit ? (uint16_t)((phase + step) & 0x3FFFU)
                : (uint16_t)((phase - step) & 0x3FFFU);
    if (cfg->phase_acc != 0) {
        *cfg->phase_acc = phase;
    }

    cmd->ftw = add_ftw_offset(cfg->center_ftw, offset);
    cmd->asf = cfg->asf;
    cmd->pow = phase;
    cmd->profile = cfg->profile;
    /* MSK writes FTW + ACR + CPOW in a single multi-register frame.
     * Phase and frequency both update every cycle for continuous phase. */
}

int MSK_GenerateFrame(const MSK_Config *cfg, uint8_t bit,
                      uint8_t *spi1_buf, uint8_t *spi3_buf)
{
    DDS_Command cmd;
    MSK_GenerateBit(cfg, bit, &cmd);
    return Encoder_FormatCommand(&cmd, spi1_buf, spi3_buf);
}
