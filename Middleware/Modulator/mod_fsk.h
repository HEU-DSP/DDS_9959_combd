/**
 ******************************************************************************
 * @file    mod_fsk.h
 * @brief   Frequency Shift Keying (FSK) Modulator
 ******************************************************************************
 */

#ifndef __MOD_FSK_H__
#define __MOD_FSK_H__

#include "dds_command.h"

typedef struct {
    uint32_t ftw_mark;    /* FTW for mark (bit 1)  */
    uint32_t ftw_space;   /* FTW for space (bit 0) */
    uint16_t asf;         /* Amplitude              */
    uint8_t  profile;     /* Target profile         */
} FSK_Config;

/**
 * @brief  Convert a bit sequence to DDS_Command array for FSK
 * @param  cfg      : FSK parameters
 * @param  bits     : bit array (LSB first per byte, or packed)
 * @param  num_bits : number of bits to convert
 * @param  cmds     : [out] DDS_Command array
 * @param  max_cmds : max capacity of cmds[]
 * @return          : actual number of commands generated
 */
int FSK_Generate(const FSK_Config *cfg,
                 const uint8_t *bits, int num_bits,
                 DDS_Command *cmds, int max_cmds);

/**
 * @brief  Generate a single FSK command for a given bit value
 */
void FSK_GenerateBit(const FSK_Config *cfg, uint8_t bit, DDS_Command *cmd);

/**
 * @brief  Generate FSK SPI frame for a single bit (DDS_Command → SPI buffer)
 * @return frame length in bytes per SPI lane
 */
int FSK_GenerateFrame(const FSK_Config *cfg, uint8_t bit,
                      uint8_t *spi1_buf, uint8_t *spi3_buf);

#endif /* __MOD_FSK_H__ */
