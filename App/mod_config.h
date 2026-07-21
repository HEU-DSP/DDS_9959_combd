/**
 ******************************************************************************
 * @file    mod_config.h
 * @brief   Per-channel modulation parameters (App-layer, global)
 *
 * Each channel has an independent ChannelModConfig containing
 * modulation-type-specific parameter structs (CW/FSK/ASK).
 * Write from App or debugger; Middleware reads to encode SPI frames.
 ******************************************************************************
 */

#ifndef __MOD_CONFIG_H__
#define __MOD_CONFIG_H__

#include <stdint.h>
#include <stdbool.h>

#define DDS_CHANNEL_COUNT  4U

typedef enum {
    CH_MODE_CW   = 0,
    CH_MODE_FSK  = 1,
    CH_MODE_ASK  = 2,
    CH_MODE_GFSK = 3,
    CH_MODE_MSK  = 4,
    CH_MODE_QPSK = 5,
    CH_MODE_AM   = 6,
    CH_MODE_FM   = 7,
    CH_MODE_BPSK = 8,
    CH_MODE_4FSK = 9,
    CH_MODE_OFF  = 0xFF,
} ChMode;

/* ---- Per-modulation-type parameter structs ---- */
typedef struct {
    uint32_t ftw;
    uint16_t asf;
} CW_Params;

typedef struct {
    uint32_t ftw_mark;
    uint32_t ftw_space;
    uint16_t asf;
} FSK_Params;

typedef struct {
    uint32_t ftw;
    uint16_t asf_on;
    uint16_t asf_off;
} ASK_Params;

typedef struct {
    uint32_t center_ftw;
    int32_t  deviation_ftw;
    uint16_t asf;
    int32_t  filter_state;  /* Q8 signed smoothing state */
} GFSK_Params;

typedef struct {
    uint32_t center_ftw;
    int32_t  deviation_ftw;
    uint16_t asf;
    uint16_t phase_step;
    uint16_t phase_acc;     /* 14-bit phase accumulator state */
} MSK_Params;

typedef struct {
    uint32_t ftw;
    uint16_t asf;
    uint16_t phase_00;
    uint16_t phase_01;
    uint16_t phase_10;
    uint16_t phase_11;
} QPSK_Params;

typedef struct {
    uint32_t ftw;
    uint16_t asf_center;
    uint16_t asf_delta;
} AM_Params;

typedef struct {
    uint32_t center_ftw;
    int32_t  deviation_ftw;
    uint16_t asf;
} FM_Params;

typedef struct {
    uint32_t ftw;
    uint16_t asf;
    uint16_t phase0;
    uint16_t phase1;
} BPSK_Params;

typedef struct {
    uint32_t ftw[4];
    uint16_t asf;
} FSK4_Params;

typedef struct {
    uint8_t   mode;      /* ChMode */
    bool      enabled;
    CW_Params  cw;
    FSK_Params fsk;
    ASK_Params ask;
    GFSK_Params gfsk;
    MSK_Params  msk;
    QPSK_Params qpsk;
    AM_Params   am;
    FM_Params   fm;
    BPSK_Params bpsk;
    FSK4_Params fsk4;
} ChannelModConfig;

extern ChannelModConfig mod_cfg[DDS_CHANNEL_COUNT];

/* ---- Convenience setters (debugger / code) ---- */
static inline void ModCfg_SetCW(uint8_t ch, uint32_t ftw, uint16_t asf) {
    mod_cfg[ch].mode = CH_MODE_CW; mod_cfg[ch].enabled = true;
    mod_cfg[ch].cw.ftw = ftw; mod_cfg[ch].cw.asf = asf;
}
static inline void ModCfg_SetFSK(uint8_t ch, uint32_t ftw_m, uint32_t ftw_s, uint16_t asf) {
    mod_cfg[ch].mode = CH_MODE_FSK; mod_cfg[ch].enabled = true;
    mod_cfg[ch].fsk.ftw_mark = ftw_m; mod_cfg[ch].fsk.ftw_space = ftw_s;
    mod_cfg[ch].fsk.asf = asf;
}
static inline void ModCfg_SetASK(uint8_t ch, uint32_t ftw, uint16_t asf_on, uint16_t asf_off) {
    mod_cfg[ch].mode = CH_MODE_ASK; mod_cfg[ch].enabled = true;
    mod_cfg[ch].ask.ftw = ftw; mod_cfg[ch].ask.asf_on = asf_on;
    mod_cfg[ch].ask.asf_off = asf_off;
}
static inline void ModCfg_SetGFSK(uint8_t ch, uint32_t center_ftw, int32_t deviation_ftw, uint16_t asf) {
    mod_cfg[ch].mode = CH_MODE_GFSK; mod_cfg[ch].enabled = true;
    mod_cfg[ch].gfsk.center_ftw = center_ftw; mod_cfg[ch].gfsk.deviation_ftw = deviation_ftw;
    mod_cfg[ch].gfsk.asf = asf; mod_cfg[ch].gfsk.filter_state = 0;
}
static inline void ModCfg_SetMSK(uint8_t ch, uint32_t center_ftw, int32_t deviation_ftw,
                                 uint16_t asf, uint16_t phase_step) {
    mod_cfg[ch].mode = CH_MODE_MSK; mod_cfg[ch].enabled = true;
    mod_cfg[ch].msk.center_ftw = center_ftw; mod_cfg[ch].msk.deviation_ftw = deviation_ftw;
    mod_cfg[ch].msk.asf = asf; mod_cfg[ch].msk.phase_step = phase_step & 0x3FFFU;
    mod_cfg[ch].msk.phase_acc = 0;
}
static inline void ModCfg_SetQPSK(uint8_t ch, uint32_t ftw, uint16_t asf) {
    mod_cfg[ch].mode = CH_MODE_QPSK; mod_cfg[ch].enabled = true;
    mod_cfg[ch].qpsk.ftw = ftw; mod_cfg[ch].qpsk.asf = asf;
    mod_cfg[ch].qpsk.phase_00 = 0x0800U; /* 45 deg */
    mod_cfg[ch].qpsk.phase_01 = 0x1800U; /* 135 deg */
    mod_cfg[ch].qpsk.phase_10 = 0x3800U; /* 315 deg */
    mod_cfg[ch].qpsk.phase_11 = 0x2800U; /* 225 deg */
}
static inline void ModCfg_SetAM(uint8_t ch, uint32_t ftw, uint16_t asf_center, uint16_t asf_delta) {
    mod_cfg[ch].mode = CH_MODE_AM; mod_cfg[ch].enabled = true;
    mod_cfg[ch].am.ftw = ftw; mod_cfg[ch].am.asf_center = asf_center; mod_cfg[ch].am.asf_delta = asf_delta;
}
static inline void ModCfg_SetFM(uint8_t ch, uint32_t center_ftw, int32_t deviation_ftw, uint16_t asf) {
    mod_cfg[ch].mode = CH_MODE_FM; mod_cfg[ch].enabled = true;
    mod_cfg[ch].fm.center_ftw = center_ftw; mod_cfg[ch].fm.deviation_ftw = deviation_ftw;
    mod_cfg[ch].fm.asf = asf;
}
static inline void ModCfg_Disable(uint8_t ch) {
    mod_cfg[ch].enabled = false; mod_cfg[ch].mode = CH_MODE_OFF;
}

static inline void ModCfg_SetBPSK(uint8_t ch,
                                  uint32_t ftw,
                                  uint16_t asf,
                                  uint16_t phase0,
                                  uint16_t phase1) {
    mod_cfg[ch].mode = CH_MODE_BPSK; 
    mod_cfg[ch].enabled = true;
    mod_cfg[ch].bpsk.ftw = ftw; 
    mod_cfg[ch].bpsk.asf = asf;
    mod_cfg[ch].bpsk.phase0 = phase0; 
    mod_cfg[ch].bpsk.phase1 = phase1;
}

static inline void ModCfg_Set4FSK(uint8_t ch,
                                  uint32_t ftw0,
                                  uint32_t ftw1,
                                  uint32_t ftw2,
                                  uint32_t ftw3,
                                  uint16_t asf) {
    mod_cfg[ch].mode = CH_MODE_4FSK;
    mod_cfg[ch].enabled = true;
    mod_cfg[ch].fsk4.ftw[0] = ftw0;
    mod_cfg[ch].fsk4.ftw[1] = ftw1;
    mod_cfg[ch].fsk4.ftw[2] = ftw2;
    mod_cfg[ch].fsk4.ftw[3] = ftw3;
    mod_cfg[ch].fsk4.asf = asf;
}

#endif /* __MOD_CONFIG_H__ */
