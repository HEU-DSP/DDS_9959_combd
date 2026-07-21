/**
 ******************************************************************************
 * @file    mod_msk.h
 * @brief   Minimum Shift Keying (MSK) Modulator
 ******************************************************************************
 */

#ifndef __MOD_MSK_H__
#define __MOD_MSK_H__

#include <stdint.h>
#include "dds_command.h"

typedef struct {
    uint32_t center_ftw;
    int32_t  deviation_ftw;
    uint16_t asf;
    uint16_t phase_step;
    uint16_t *phase_acc;   /* persistent 14-bit phase state */
    uint8_t  profile;
} MSK_Config;

void MSK_GenerateBit(const MSK_Config *cfg, uint8_t bit, DDS_Command *cmd);
int MSK_GenerateFrame(const MSK_Config *cfg, uint8_t bit,
                      uint8_t *spi1_buf, uint8_t *spi3_buf);

#endif /* __MOD_MSK_H__ */
