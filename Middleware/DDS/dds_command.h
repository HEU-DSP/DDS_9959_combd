/**
 ******************************************************************************
 * @file    dds_command.h
 * @brief   DDS_Command Unified Data Structure
 ******************************************************************************
 */

#ifndef __DDS_COMMAND_H__
#define __DDS_COMMAND_H__

#include <stdint.h>

typedef enum {
    DDS_UPDATE_FTW,
    DDS_UPDATE_ASF,
    DDS_UPDATE_POW
} DDS_UpdateKind;

typedef struct {
    uint32_t ftw;        /* Frequency Tuning Word (32-bit)
                            f_out = ftw / 2^32 * SYSCLK         */
    uint16_t asf;        /* Amplitude Scale Factor (10-bit)
                            0 = off, 0x3FF = full scale          */
    uint16_t pow;        /* Phase Offset Word (14-bit)
                            0–16383                             */
    uint8_t  profile;    /* Target Profile pin (0–7)             */
    DDS_UpdateKind update_kind; /* Register written by this DMA frame */
} DDS_Command;

/**
 * @brief  Calculate Frequency Tuning Word from Hz
 * @param  freq_hz   : desired output frequency in Hz
 * @param  sysclk_hz : AD9959 SYSCLK in Hz (e.g. 500000000)
 * @return 32-bit FTW
 */
static inline uint32_t DDS_CalcFTW(uint32_t freq_hz, uint32_t sysclk_hz)
{
    return (uint32_t)((double)freq_hz / sysclk_hz * (1ULL << 32));
}

#endif /* __DDS_COMMAND_H__ */
