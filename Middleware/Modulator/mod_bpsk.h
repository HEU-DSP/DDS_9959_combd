/**
 ******************************************************************************
 * @file    mod_bpsk.h
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
#ifndef __MOD_BPSK_H__
#define __MOD_BPSK_H__

#include "dds_command.h"

typedef struct {
    uint32_t ftw;
    uint16_t asf;
    uint16_t phase0;
    uint16_t phase1;
    uint8_t  profile;
} BPSK_Config;

/**
 * @brief  Generate a single BPSK DDS_Command
 * @param  ftw      : frequency tuning word
 * @param  asf      : amplitude (0–0x3FF)
 * @param  phase0   : phase for bit=0 (0–0x3FFF)
 * @param  phase1   : phase for bit=1 (0–0x3FFF)
 * @param  profile  : target profile (0–7)
 * @param  cmd      : [out] DDS_Command filled
 */
void BPSK_GenerateBit(const BPSK_Config * cfg,
                      uint8_t bit,
                      DDS_Command *cmd);

int BPSK_GenerateFrame(const BPSK_Config *cfg,
                       uint8_t bit,
                       uint8_t *spi1_buf,
                       uint8_t *spi3_buf);

#endif /* __MOD_BPSK_H__ */
