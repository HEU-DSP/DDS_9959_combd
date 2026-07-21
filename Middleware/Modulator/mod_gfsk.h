/**
 ******************************************************************************
 * @file    mod_gfsk.h
 * @brief   Gaussian Frequency Shift Keying (GFSK) Modulator
 ******************************************************************************
 */

#ifndef __MOD_GFSK_H__
#define __MOD_GFSK_H__

#include <stdint.h>
#include "dds_command.h"

typedef struct {
    uint32_t center_ftw;
    int32_t  deviation_ftw;
    uint16_t asf;
    int32_t *filter_state;  /* Q8 signed smoothing state */
    uint8_t  profile;
} GFSK_Config;

void GFSK_GenerateBit(const GFSK_Config *cfg, uint8_t bit, DDS_Command *cmd);
int GFSK_GenerateFrame(const GFSK_Config *cfg, uint8_t bit,
                       uint8_t *spi1_buf, uint8_t *spi3_buf);

#endif /* __MOD_GFSK_H__ */
