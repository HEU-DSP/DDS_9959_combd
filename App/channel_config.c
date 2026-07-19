/**
 ******************************************************************************
 * @file    channel_config.c
 * @brief   Build encoded SPI frames from mod_cfg + sym_buf into tx_bank
 ******************************************************************************
 */

#include "channel_config.h"
#include "mod_config.h"
#include "symbol_buffer.h"
#include "frame_builder.h"
#include "dds_encoder.h"
#include <string.h>

/* Current AD9959 command frames are about 14 bytes per SPI lane. Keep a small
 * guard band so the ISR refill path never writes past the fixed bank size. */
#define ENCODER_SAFE_FRAME_BYTES  16U

static bool has_enabled_symbol_mod(void)
{
    for (uint8_t ch = 0; ch < DDS_CHANNEL_COUNT; ch++) {
        if (!mod_cfg[ch].enabled) {
            continue;
        }
        if ((mod_cfg[ch].mode == CH_MODE_FSK) ||
            (mod_cfg[ch].mode == CH_MODE_ASK)) {
            return true;
        }
    }
    return false;
}

static int build_channel_frame(uint8_t ch, bool have_bit, uint8_t bit,
                               uint8_t *spi1_buf, uint8_t *spi3_buf)
{
    switch ((ChMode)mod_cfg[ch].mode) {
    case CH_MODE_CW:
        return FrameBuilder_CW(spi1_buf, spi3_buf,
                               mod_cfg[ch].cw.ftw,
                               mod_cfg[ch].cw.asf,
                               ch);

    case CH_MODE_FSK:
        if (!have_bit) {
            return 0;
        }
        {
            FSK_Config cfg = {
                .ftw_mark = mod_cfg[ch].fsk.ftw_mark,
                .ftw_space = mod_cfg[ch].fsk.ftw_space,
                .asf = mod_cfg[ch].fsk.asf,
                .profile = ch,
            };
            return FrameBuilder_FSK(spi1_buf, spi3_buf, &cfg, bit);
        }

    case CH_MODE_ASK:
        if (!have_bit) {
            return 0;
        }
        {
            ASK_Config cfg = {
                .ftw = mod_cfg[ch].ask.ftw,
                .asf_on = mod_cfg[ch].ask.asf_on,
                .asf_off = mod_cfg[ch].ask.asf_off,
                .profile = ch,
            };
            return FrameBuilder_ASK(spi1_buf, spi3_buf, &cfg, bit);
        }

    default:
        return 0;
    }
}

static bool append_enabled_channels(FrameBank *bank, uint16_t *total,
                                    bool have_bit, uint8_t bit)
{
    bool wrote_any = false;

    /* A single symbol period may update multiple AD9959 channels, so all
     * enabled channels are appended into the same DMA bank contiguously. */
    for (uint8_t ch = 0; ch < DDS_CHANNEL_COUNT; ch++) {
        if (!mod_cfg[ch].enabled) {
            continue;
        }
        if ((FRAME_BUF_SIZE - *total) < ENCODER_SAFE_FRAME_BYTES) {
            break;
        }

        uint8_t *s1 = bank->spi1 + *total;
        uint8_t *s3 = bank->spi3 + *total;
        int len = build_channel_frame(ch, have_bit, bit, s1, s3);
        if (len <= 0) {
            continue;
        }

        *total += (uint16_t)len;
        wrote_any = true;
    }

    return wrote_any;
}

bool Encoder_BuildBank(FrameBank *bank, uint16_t *out_bank_bytes)
{
    if ((bank == 0) || (out_bank_bytes == 0)) {
        return false;
    }

    uint16_t total = 0;
    memset(bank, 0, sizeof(*bank));

    /* CW does not consume symbols: rebuild one fixed carrier update and let
     * the hardware layer repeat banks until App changes the configuration. */
    if (!has_enabled_symbol_mod()) {
        append_enabled_channels(bank, &total, false, 0U);
        *out_bank_bytes = total;
        return (total > 0U);
    }

    /* Symbol modulation consumes as many queued bits as fit in this bank.
     * Remaining bits stay in symbol_buffer for the next idle-bank refill. */
    while (SymbolBuf_HasData() &&
           ((FRAME_BUF_SIZE - total) >= ENCODER_SAFE_FRAME_BYTES)) {
        uint8_t bit = 0;
        if (!SymbolBuf_ReadBit(&bit)) {
            break;
        }

        if (!append_enabled_channels(bank, &total, true, bit)) {
            break;
        }
    }

    *out_bank_bytes = total;
    return (total > 0U);
}
