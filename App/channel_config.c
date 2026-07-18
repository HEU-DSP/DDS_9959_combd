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
#include "ad9959_reg.h"

bool Encoder_BuildBank(FrameBank *bank, uint16_t *out_bank_bytes)
{
    int total = 0;
    bool consumed_sym = false;

    for (uint8_t ch = 0; ch < DDS_CHANNEL_COUNT; ch++) {
        if (!mod_cfg[ch].enabled) continue;

        uint8_t *s1 = bank->spi1 + total;
        uint8_t *s3 = bank->spi3 + total;
        int len = 0;

        switch ((ChMode)mod_cfg[ch].mode) {
        case CH_MODE_CW:
            /* CW: always build regardless of sym_buf */
            len = FrameBuilder_CW(s1, s3, mod_cfg[ch].cw.ftw,
                                  mod_cfg[ch].fsk.asf, ch);
            break;
        case CH_MODE_FSK:
            if (sym_buf.ready && sym_buf.count > 0) {
                uint8_t bit = sym_buf.data[0] & 1;
                uint32_t ftw = bit ? mod_cfg[ch].fsk.ftw_mark
                                   : mod_cfg[ch].fsk.ftw_space;
                DDS_Command cmd = { .ftw = ftw, .asf = mod_cfg[ch].fsk.asf,
                                    .pow = 0, .profile = ch };
                len = Encoder_FormatCommand(&cmd, s1, s3);
                consumed_sym = true;
            }
            break;
        case CH_MODE_ASK:
            if (sym_buf.ready && sym_buf.count > 0) {
                uint8_t bit = sym_buf.data[0] & 1;
                uint16_t asf = bit ? mod_cfg[ch].ask.asf_on
                                   : mod_cfg[ch].ask.asf_off;
                DDS_Command cmd = { .ftw = mod_cfg[ch].ask.ftw, .asf = asf,
                                    .pow = 0, .profile = ch };
                len = Encoder_FormatCommand(&cmd, s1, s3);
                consumed_sym = true;
            }
            break;
        default:
            break;
        }
        total += len;
        if ((uint16_t)total >= FRAME_BUF_SIZE) break;
    }

    /* Consume one symbol if we used it */
    if (consumed_sym) {
        if (sym_buf.count > 0) sym_buf.count--;
        if (sym_buf.count == 0) {
            sym_buf.ready = false;
            sym_buf.free  = true;
        }
    }

    *out_bank_bytes = (uint16_t)total;
    return (total > 0);
}
