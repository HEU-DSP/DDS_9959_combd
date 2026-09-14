/**
 ******************************************************************************
 * @file    dds_command.h
 * @brief   DDS_Command Unified Data Structure
 ******************************************************************************
 */

#ifndef __DDS_COMMAND_H__
#define __DDS_COMMAND_H__

#include <stdint.h>

typedef struct {
    uint32_t ftw;        /* Frequency Tuning Word (32-bit)
                            f_out = ftw / 2^32 * SYSCLK         */
    uint16_t asf;        /* Amplitude Scale Factor (10-bit)
                            0 = off, 0x3FF = full scale          */
    uint16_t pow;        /* Phase Offset Word (14-bit)
                            0–16383                             */
    uint8_t  profile;    /* Target channel (0–3)                 */
} DDS_Command;

/* FTW computation: DDSCalc_FTW(freq_hz) in Middleware/Modulator/dds_calc.h
 * (Q32.32 multiply-shift, no division).  The deprecated float-based
 * DDS_CalcFTW was removed 2026-08-08. */

#endif /* __DDS_COMMAND_H__ */
