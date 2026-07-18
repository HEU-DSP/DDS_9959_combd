/**
 ******************************************************************************
 * @file    mod_cw.h
 * @brief   Continuous Wave (CW) Modulator
 ******************************************************************************
 */

#ifndef __MOD_CW_H__
#define __MOD_CW_H__

#include "dds_command.h"

/**
 * @brief  Generate a single CW DDS_Command (constant carrier)
 * @param  ftw      : frequency tuning word
 * @param  asf      : amplitude (0–0x3FF)
 * @param  profile  : target profile (0–7)
 * @param  cmd      : [out] DDS_Command filled
 */
void CW_Generate(uint32_t ftw, uint16_t asf, uint8_t profile,
                 DDS_Command *cmd);

/**
 * @brief  Generate CW SPI frame directly (DDS_Command → SPI buffer)
 */
int CW_GenerateFrame(uint32_t ftw, uint16_t asf, uint8_t profile,
                     uint8_t *spi1_buf, uint8_t *spi3_buf);

#endif /* __MOD_CW_H__ */
