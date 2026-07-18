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
    CH_MODE_CW  = 0,
    CH_MODE_FSK = 1,
    CH_MODE_ASK = 2,
    CH_MODE_OFF = 0xFF,
} ChMode;

/* ---- Per-modulation-type parameter structs ---- */
typedef struct {
    uint32_t ftw;
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
    uint8_t   mode;      /* ChMode */
    bool      enabled;
    CW_Params  cw;
    FSK_Params fsk;
    ASK_Params ask;
} ChannelModConfig;

extern ChannelModConfig mod_cfg[DDS_CHANNEL_COUNT];

/* ---- Convenience setters (debugger / code) ---- */
static inline void ModCfg_SetCW(uint8_t ch, uint32_t ftw, uint16_t asf) {
    mod_cfg[ch].mode = CH_MODE_CW; mod_cfg[ch].enabled = true;
    mod_cfg[ch].cw.ftw = ftw; mod_cfg[ch].fsk.asf = asf;
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
static inline void ModCfg_Disable(uint8_t ch) {
    mod_cfg[ch].enabled = false; mod_cfg[ch].mode = CH_MODE_OFF;
}

#endif /* __MOD_CONFIG_H__ */
