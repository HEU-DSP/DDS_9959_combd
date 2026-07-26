/**
 ******************************************************************************
 * @file    mod_ask.c
 * @brief   ASK Modulator Implementation
 ******************************************************************************
 */

#include "mod_ask.h"
#include "dds_encoder.h"

void ASK_GenerateBit(const ASK_Config *cfg, uint8_t bit, DDS_Command *cmd)
{
    cmd->ftw     = cfg->ftw;
    cmd->asf     = (bit) ? cfg->asf_on : cfg->asf_off;
    cmd->pow     = 0;
    cmd->profile = cfg->profile;
}

int ASK_Generate(const ASK_Config *cfg,
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
            ASK_GenerateBit(cfg, bit, &cmds[count]);
            count++;
        }
    }
    return count;
}

int ASK_GenerateFrame(const ASK_Config *cfg, uint8_t bit,
                      uint8_t *spi1_buf, uint8_t *spi3_buf)
{
    DDS_Command cmd;
    ASK_GenerateBit(cfg, bit, &cmd);
    return Encoder_FormatCommand(&cmd, spi1_buf, spi3_buf);
}
