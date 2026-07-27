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
#include "phase1_config.h"
#include <string.h>

/* Multi-register full frame: CSR(2) + CFTW(5) + ACR(4) + CPOW(3) = 14 bytes. */
#define ENCODER_SAFE_FRAME_BYTES  ENCODER_FRAME_BYTES

static uint32_t static_symbol_index = 0U;

typedef struct {
    bool    have_bit;
    uint8_t bit;

    bool    have_symbol;
    uint8_t symbol;

    bool    have_sample;
    int16_t sample;
} ModInput;

static uint8_t mode_input_bits(ChMode mode)
{
    switch (mode) {
    case CH_MODE_FSK:
    case CH_MODE_ASK:
    case CH_MODE_GFSK:
    case CH_MODE_MSK:
    case CH_MODE_AM:
    case CH_MODE_FM:
    case CH_MODE_BPSK:
        return 1U;

    case CH_MODE_QPSK:
    case CH_MODE_4FSK:
        return 2U;

    default:
        return 0U;
    }
}

static uint8_t enabled_input_bits(void)
{
    uint8_t bits = 0U;

    for (uint8_t ch = 0; ch < DDS_CHANNEL_COUNT; ch++) {
        if (!mod_cfg[ch].enabled) {
            continue;
        }
        uint8_t ch_bits = mode_input_bits((ChMode)mod_cfg[ch].mode);
        if (ch_bits > bits) {
            bits = ch_bits;
        }
    }
    return bits;
}

static void make_mod_input(uint8_t raw_symbol, uint8_t input_bits,
                           ModInput *input)
{
    memset(input, 0, sizeof(*input));

    if (input_bits == 0U) {
        return;
    }

    /* Bit and symbol currently come from the existing SymbolBuffer bit stream.
     * TODO: Split a real BitBuffer/SymbolBuffer when packet/symbol mapping is
     * implemented, so bit-mode and symbol-mode channels can consume independent
     * input units instead of sharing raw_symbol. */
    input->have_bit = true;
    input->bit = raw_symbol & 0x01U;
    input->have_symbol = true;
    input->symbol = raw_symbol;

    /* AM/FM currently use a temporary 1-bit sample mapping for bring-up.
     * TODO: Replace this with SampleBuffer_Read(&sample) when analog/sample
     * stream input is implemented. */
    input->have_sample = true;
    input->sample = input->bit ? 32767 : -32767;
}

static int build_channel_frame(uint8_t ch, const ModInput *input,
                               uint8_t *spi1_buf, uint8_t *spi3_buf)
{
    switch ((ChMode)mod_cfg[ch].mode) {
    case CH_MODE_CW:
        return FrameBuilder_CW(spi1_buf, spi3_buf,
                               mod_cfg[ch].cw.ftw,
                               mod_cfg[ch].cw.asf,
                               ch);

    case CH_MODE_FSK:
        if (!input->have_bit) {
            return 0;
        }
        {
            FSK_Config cfg = {
                .ftw_mark = mod_cfg[ch].fsk.ftw_mark,
                .ftw_space = mod_cfg[ch].fsk.ftw_space,
                .asf = mod_cfg[ch].fsk.asf,
                .profile = ch,
            };
            return FrameBuilder_FSK(spi1_buf, spi3_buf, &cfg, input->bit);
        }

    case CH_MODE_ASK:
        if (!input->have_bit) {
            return 0;
        }
        {
            ASK_Config cfg = {
                .ftw = mod_cfg[ch].ask.ftw,
                .asf_on = mod_cfg[ch].ask.asf_on,
                .asf_off = mod_cfg[ch].ask.asf_off,
                .profile = ch,
            };
            return FrameBuilder_ASK(spi1_buf, spi3_buf, &cfg, input->bit);
        }

    case CH_MODE_GFSK:
        if (!input->have_bit) {
            return 0;
        }
        {
            GFSK_Config cfg = {
                .center_ftw = mod_cfg[ch].gfsk.center_ftw,
                .deviation_ftw = mod_cfg[ch].gfsk.deviation_ftw,
                .asf = mod_cfg[ch].gfsk.asf,
                .filter_state = &mod_cfg[ch].gfsk.filter_state,
                .profile = ch,
            };
            return FrameBuilder_GFSK(spi1_buf, spi3_buf, &cfg, input->bit);
        }

    case CH_MODE_MSK:
        if (!input->have_bit) {
            return 0;
        }
        {
            MSK_Config cfg = {
                .center_ftw = mod_cfg[ch].msk.center_ftw,
                .deviation_ftw = mod_cfg[ch].msk.deviation_ftw,
                .asf = mod_cfg[ch].msk.asf,
                .phase_step = mod_cfg[ch].msk.phase_step,
                .phase_acc = &mod_cfg[ch].msk.phase_acc,
                .profile = ch,
            };
            return FrameBuilder_MSK(spi1_buf, spi3_buf, &cfg, input->bit);
        }

    case CH_MODE_QPSK:
        if (!input->have_symbol) {
            return 0;
        }
        {
            QPSK_Config cfg = {
                .ftw = mod_cfg[ch].qpsk.ftw,
                .asf = mod_cfg[ch].qpsk.asf,
                .phase_00 = mod_cfg[ch].qpsk.phase_00,
                .phase_01 = mod_cfg[ch].qpsk.phase_01,
                .phase_10 = mod_cfg[ch].qpsk.phase_10,
                .phase_11 = mod_cfg[ch].qpsk.phase_11,
                .profile = ch,
            };
            return FrameBuilder_QPSK(spi1_buf, spi3_buf, &cfg, input->symbol);
        }

    case CH_MODE_4FSK:
        if (!input->have_symbol) {
            return 0;
        }
        {
            FSK4_Config cfg = {
                .ftw = {
                    mod_cfg[ch].fsk4.ftw[0],
                    mod_cfg[ch].fsk4.ftw[1],
                    mod_cfg[ch].fsk4.ftw[2],
                    mod_cfg[ch].fsk4.ftw[3],
                },
                .asf = mod_cfg[ch].fsk4.asf,
                .profile = ch,
            };
            return FrameBuilder_4FSK(spi1_buf, spi3_buf, &cfg, input->symbol);
        }

    case CH_MODE_AM:
        if (!input->have_sample) {
            return 0;
        }
        {
            AM_Config cfg = {
                .ftw = mod_cfg[ch].am.ftw,
                .asf_center = mod_cfg[ch].am.asf_center,
                .asf_delta = mod_cfg[ch].am.asf_delta,
                .profile = ch,
            };
            return FrameBuilder_AM(spi1_buf, spi3_buf, &cfg, input->sample);
        }

    case CH_MODE_FM:
        if (!input->have_sample) {
            return 0;
        }
        {
            FM_Config cfg = {
                .center_ftw = mod_cfg[ch].fm.center_ftw,
                .deviation_ftw = mod_cfg[ch].fm.deviation_ftw,
                .asf = mod_cfg[ch].fm.asf,
                .profile = ch,
            };
            return FrameBuilder_FM(spi1_buf, spi3_buf, &cfg, input->sample);
        }

    case CH_MODE_BPSK:
        if (!input->have_bit) {
            return 0;
        }
        {
            BPSK_Config cfg = {
                .ftw = mod_cfg[ch].bpsk.ftw,
                .asf = mod_cfg[ch].bpsk.asf,
                .phase0 = mod_cfg[ch].bpsk.phase0,
                .phase1 = mod_cfg[ch].bpsk.phase1,
                .profile = ch,
            };
            return FrameBuilder_BPSK(spi1_buf, spi3_buf, &cfg, input->bit);
        }
    default:
        return 0;
    }
}

static bool append_enabled_channels(FrameBank *bank, uint16_t *total,
                                    const ModInput *input)
{
    /* One DMA start = one CS-low window = one full multi-register frame.
     * Do not append a second channel to this bank. */
    for (uint8_t ch = 0; ch < DDS_CHANNEL_COUNT; ch++) {
        if (!mod_cfg[ch].enabled) {
            continue;
        }
        if ((FRAME_BUF_SIZE - *total) < ENCODER_SAFE_FRAME_BYTES) {
            break;
        }

        uint8_t *s1 = bank->spi1 + *total;
        uint8_t *s3 = bank->spi3 + *total;
        int len = build_channel_frame(ch, input, s1, s3);
        if (len <= 0) {
            continue;
        }

        *total += (uint16_t)len;
        return true;
    }

    return false;
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
    uint8_t input_bits = enabled_input_bits();
    if (input_bits == 0U) {
        ModInput input;
        make_mod_input(0U, 0U, &input);
        append_enabled_channels(bank, &total, &input);
        *out_bank_bytes = total;
        return (total > 0U);
    }

    if ((FRAME_BUF_SIZE - total) >= ENCODER_SAFE_FRAME_BYTES) {
        uint8_t raw_symbol = 0;
        ModInput input;
#if AD9959_STATIC_SYMBOL_SOURCE_ENABLE
        /* Encoder_BuildBank is called to prepare exactly one following DMA
         * frame.  This source is therefore frame/timer paced, not main-loop
         * paced: 1-bit -> 0,1,...; 2-bit -> 0,1,2,3,... */
        raw_symbol = (uint8_t)(static_symbol_index++ &
                               ((1UL << input_bits) - 1UL));
#else
        if (!SymbolBuf_HasData()) {
            *out_bank_bytes = total;
            return false;
        }
        if (!SymbolBuf_ReadBits(&raw_symbol, input_bits)) {
            *out_bank_bytes = total;
            return false;
        }
#endif
        make_mod_input(raw_symbol, input_bits, &input);

        (void)append_enabled_channels(bank, &total, &input);
    }

    *out_bank_bytes = total;
    return (total > 0U);
}
