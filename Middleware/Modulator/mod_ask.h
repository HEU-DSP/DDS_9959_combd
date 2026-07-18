/**
 ******************************************************************************
 * @file    mod_ask.h
 * @brief   Amplitude Shift Keying (ASK) Modulator
 ******************************************************************************
 */

#ifndef __MOD_ASK_H__
#define __MOD_ASK_H__

#include "dds_command.h"

typedef struct {
    uint32_t ftw;         /* Carrier FTW             */
    uint16_t asf_on;      /* Amplitude for bit 1     */
    uint16_t asf_off;     /* Amplitude for bit 0     */
    uint8_t  profile;     /* Target profile          */
} ASK_Config;

int  ASK_Generate(const ASK_Config *cfg,
                  const uint8_t *bits, int num_bits,
                  DDS_Command *cmds, int max_cmds);
void ASK_GenerateBit(const ASK_Config *cfg, uint8_t bit, DDS_Command *cmd);

int  ASK_GenerateFrame(const ASK_Config *cfg, uint8_t bit,
                       uint8_t *spi1_buf, uint8_t *spi3_buf);

#endif /* __MOD_ASK_H__ */
