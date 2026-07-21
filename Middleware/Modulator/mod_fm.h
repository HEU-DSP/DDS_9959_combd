/**
 ******************************************************************************
 * @file    mod_fm.h
 * @brief   Frequency Modulation (FM) Modulator
 ******************************************************************************
 */

#ifndef __MOD_FM_H__
#define __MOD_FM_H__

#include <stdint.h>
#include "dds_command.h"

typedef struct {
    uint32_t center_ftw;
    int32_t  deviation_ftw;
    uint16_t asf;
    uint8_t  profile;
} FM_Config;

void FM_GenerateSample(const FM_Config *cfg, int16_t sample, DDS_Command *cmd);
int FM_GenerateFrame(const FM_Config *cfg, int16_t sample,
                     uint8_t *spi1_buf, uint8_t *spi3_buf);

#endif /* __MOD_FM_H__ */
