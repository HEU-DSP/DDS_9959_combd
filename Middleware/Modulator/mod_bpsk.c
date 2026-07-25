/**
 ******************************************************************************
 * @file    mod_bpsk.c
 * @author  Jason
 * @version V1.0.0
 * @date    2026-07-21
 * @brief   Binary Phase Shift Keying (BPSK) Modulator
 ******************************************************************************
 * @attention
 *
 * 
 *
 ******************************************************************************
 */
#include "mod_bpsk.h"
#include "dds_encoder.h"

void BPSK_GenerateBit(const BPSK_Config *cfg,
                      uint8_t bit,
                      DDS_Command *cmd)
{
    cmd->ftw = cfg->ftw;
    cmd->asf = cfg->asf;
    cmd->pow = bit ? cfg->phase1 : cfg->phase0;
    cmd->profile = cfg->profile;
    cmd->update_kind = DDS_UPDATE_POW;
}

int BPSK_GenerateFrame(const BPSK_Config *cfg,
                       uint8_t bit,
                       uint8_t *spi1_buf,
                       uint8_t *spi3_buf)
{
    DDS_Command cmd;
    BPSK_GenerateBit(cfg, bit, &cmd);
    return Encoder_FormatCommand(&cmd, spi1_buf, spi3_buf);
}
