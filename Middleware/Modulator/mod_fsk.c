/**
 ******************************************************************************
 * @file    mod_fsk.c
 * @brief   FSK Modulator Implementation
 ******************************************************************************
 */

#include "mod_fsk.h"
#include "dds_encoder.h"

void FSK_GenerateBit(const FSK_Config *cfg, uint8_t bit, DDS_Command *cmd)
{
    cmd->ftw     = (bit) ? cfg->ftw_mark : cfg->ftw_space;
    cmd->asf     = cfg->asf;
    cmd->pow     = 0;
    cmd->profile = cfg->profile;
}

int FSK_Generate(const FSK_Config *cfg,
                 const uint8_t *bits, int num_bits,
                 DDS_Command *cmds, int max_cmds)
{
    int count = 0;
    for (int byte_idx = 0; byte_idx < (num_bits + 7) / 8 && count < max_cmds; byte_idx++) {
        uint8_t byte_val = bits[byte_idx];
        for (int bit_pos = 0; bit_pos < 8 && count < max_cmds; bit_pos++) {
            int global_bit = byte_idx * 8 + bit_pos;
            if (global_bit >= num_bits) break;
            uint8_t bit = (byte_val >> bit_pos) & 0x01;
            FSK_GenerateBit(cfg, bit, &cmds[count]);
            count++;
        }
    }
    return count;
}

int FSK_GenerateFrame(const FSK_Config *cfg, uint8_t bit,
                      uint8_t *spi1_buf, uint8_t *spi3_buf)
{
    DDS_Command cmd;
    FSK_GenerateBit(cfg, bit, &cmd);
    return Encoder_FormatCommand(&cmd, spi1_buf, spi3_buf);
}
