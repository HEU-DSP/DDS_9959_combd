/**
 ******************************************************************************
 * @file    mod_am.h
 * @brief   Amplitude Modulation (AM) Modulator
 ******************************************************************************
 */

#ifndef __MOD_AM_H__
#define __MOD_AM_H__

#include <stdint.h>
#include "dds_command.h"

typedef struct {
    uint32_t ftw;
    uint16_t asf_center;
    uint16_t asf_delta;
    uint8_t  profile;
} AM_Config;

void AM_GenerateSample(const AM_Config *cfg, int16_t sample, DDS_Command *cmd);
int AM_GenerateFrame(const AM_Config *cfg, int16_t sample,
                     uint8_t *spi1_buf, uint8_t *spi3_buf);

#endif /* __MOD_AM_H__ */
