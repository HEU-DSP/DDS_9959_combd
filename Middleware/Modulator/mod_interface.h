/**
 ******************************************************************************
 * @file    mod_interface.h
 * @brief   Common Modulator Interface
 *
 * All modulation modules implement:
 *   Modulator_Init(const ModConfig *cfg)
 *   Modulator_Process(const Symbol *sym, DDS_Command *out, int max_cmds)
 ******************************************************************************
 */

#ifndef __MOD_INTERFACE_H__
#define __MOD_INTERFACE_H__

#include "dds_command.h"

typedef enum {
    MOD_TYPE_CW  = 0,
    MOD_TYPE_FSK = 1,
    MOD_TYPE_ASK = 2,
    MOD_TYPE_BPSK = 3,
    MOD_TYPE_QPSK = 4,
    MOD_TYPE_GFSK = 5,
    MOD_TYPE_MSK  = 6,
} ModType;

typedef struct {
    ModType   type;
    uint32_t  sample_rate;   /* Symbol rate (Hz) */
    uint32_t  ftw_base;      /* Base frequency FTW */
    uint16_t  asf;           /* Amplitude */
    /* Modulation-specific fields extend this */
} ModConfig;

#endif /* __MOD_INTERFACE_H__ */
