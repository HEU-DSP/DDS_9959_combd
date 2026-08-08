/**
 ******************************************************************************
 * @file    channel_config.c
 * @brief   Build pre-encoded DDS_EncodedFrame structs from mod_cfg + sym_buf
 ******************************************************************************
            |
            \
        ___  \
       /   |  >
       |     /
       \ ___/
    
        ↑一团猫毛

 */

#include "channel_config.h"
#include "mod_config.h"
#include "symbol_buffer.h"
#include "dds_encoder.h"
#include "tx_buffer.h"
#include "phase1_config.h"
#include <string.h>

/* ---- Pre-encoded frame storage (replayed by TIM8 ISR) ---- */
DDS_EncodedFrame pre_encoded[DDS_CHANNEL_COUNT];
uint8_t pre_encoded_mask = 0U;

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
        if (!mod_cfg[ch].enabled) continue;
        uint8_t ch_bits = mode_input_bits((ChMode)mod_cfg[ch].mode);
        if (ch_bits > bits) bits = ch_bits;
    }
    return bits;
}

static void make_mod_input(uint8_t raw_symbol, uint8_t input_bits,
                           ModInput *input)
{
    memset(input, 0, sizeof(*input));
    if (input_bits == 0U) return;
    input->have_bit = true;
    input->bit = raw_symbol & 0x01U;
    input->have_symbol = true;
    input->symbol = raw_symbol;
    input->have_sample = true;
    input->sample = input->bit ? 32767 : -32767;
}

/**
 * @brief  Compute the DDS_Command for one channel from its current mod_cfg.
 * @return true if the command is valid, false if channel disabled / no input.
 */
static bool build_channel_command(uint8_t ch, const ModInput *input,
                                  DDS_Command *cmd)
{
    memset(cmd, 0, sizeof(*cmd));
    cmd->profile = ch;

    switch ((ChMode)mod_cfg[ch].mode) {
    case CH_MODE_CW:
        cmd->ftw = mod_cfg[ch].cw.ftw;
        cmd->asf = mod_cfg[ch].cw.asf;
        cmd->pow = 0;
        break;

    case CH_MODE_FSK:
        if (!input->have_bit) return false;
        cmd->ftw = input->bit ? mod_cfg[ch].fsk.ftw_mark
                              : mod_cfg[ch].fsk.ftw_space;
        cmd->asf = mod_cfg[ch].fsk.asf;
        break;

    case CH_MODE_ASK:
        if (!input->have_bit) return false;
        cmd->ftw = mod_cfg[ch].ask.ftw;
        cmd->asf = input->bit ? mod_cfg[ch].ask.asf_on
                              : mod_cfg[ch].ask.asf_off;
        break;

    case CH_MODE_GFSK:
        if (!input->have_bit) return false;
        {
            /* Simple 1st-order Gaussian filter on the frequency deviation. */
            int32_t target = input->bit ? mod_cfg[ch].gfsk.deviation_ftw
                                        : -mod_cfg[ch].gfsk.deviation_ftw;
            int32_t state  = mod_cfg[ch].gfsk.filter_state;
            state = (state * 3 + target) / 4;   /* α = ¼  */
            mod_cfg[ch].gfsk.filter_state = state;
            cmd->ftw = (uint32_t)((int32_t)mod_cfg[ch].gfsk.center_ftw + state);
            cmd->asf = mod_cfg[ch].gfsk.asf;
        }
        break;

    case CH_MODE_MSK:
        if (!input->have_bit) return false;
        {
            mod_cfg[ch].msk.phase_acc += mod_cfg[ch].msk.phase_step;
            uint16_t phi = mod_cfg[ch].msk.phase_acc & 0x3FFFU;
            /* Continuous-phase binary FSK at h=0.5: phase wraps through 0–2π. */
            int32_t dftw = mod_cfg[ch].msk.deviation_ftw;
            int32_t cos_val = (int32_t)((phi < 0x2000U) ? (0x2000U - phi)
                                                        : (phi - 0x2000U));
            cmd->ftw = (uint32_t)((int32_t)mod_cfg[ch].msk.center_ftw
                                  + (dftw * cos_val) / 0x2000);
            cmd->asf = mod_cfg[ch].msk.asf;
        }
        break;

    case CH_MODE_QPSK:
        if (!input->have_symbol) return false;
        cmd->ftw = mod_cfg[ch].qpsk.ftw;
        cmd->asf = mod_cfg[ch].qpsk.asf;
        switch (input->symbol & 0x03U) {
        case 0: cmd->pow = mod_cfg[ch].qpsk.phase_00; break;
        case 1: cmd->pow = mod_cfg[ch].qpsk.phase_01; break;
        case 2: cmd->pow = mod_cfg[ch].qpsk.phase_10; break;
        case 3: cmd->pow = mod_cfg[ch].qpsk.phase_11; break;
        }
        break;

    case CH_MODE_4FSK:
        if (!input->have_symbol) return false;
        {
            uint8_t sym = input->symbol & 0x03U;
            cmd->ftw = mod_cfg[ch].fsk4.ftw[sym];
            cmd->asf = mod_cfg[ch].fsk4.asf;
        }
        break;

    case CH_MODE_AM:
        if (!input->have_sample) return false;
        cmd->ftw = mod_cfg[ch].am.ftw;
        {
            int32_t amplitude = (int32_t)mod_cfg[ch].am.asf_center
                              + (int32_t)mod_cfg[ch].am.asf_delta
                              * input->sample / 32767;
            if (amplitude < 0) amplitude = 0;
            if (amplitude > 0x3FF) amplitude = 0x3FF;
            cmd->asf = (uint16_t)amplitude;
        }
        break;

    case CH_MODE_FM:
        if (!input->have_sample) return false;
        cmd->ftw = (uint32_t)((int32_t)mod_cfg[ch].fm.center_ftw
                   + (int32_t)(mod_cfg[ch].fm.deviation_ftw)
                   * input->sample / 32767);
        cmd->asf = mod_cfg[ch].fm.asf;
        break;

    case CH_MODE_BPSK:
        if (!input->have_bit) return false;
        cmd->ftw = mod_cfg[ch].bpsk.ftw;
        cmd->asf = mod_cfg[ch].bpsk.asf;
        cmd->pow = input->bit ? mod_cfg[ch].bpsk.phase1
                              : mod_cfg[ch].bpsk.phase0;
        break;

    default:
        return false;   /* CH_MODE_OFF or unrecognised */
    }

    return true;
}

/**
 * @brief  Encode one channel's current mod_cfg into a DDS_EncodedFrame.
 * @return true if a frame was written, false if channel disabled / no input.
 */
static bool build_channel_frame(uint8_t ch, const ModInput *input,
                                DDS_EncodedFrame *frame)
{
    DDS_Command cmd;
    if (!build_channel_command(ch, input, &cmd)) {
        return false;
    }
    Encoder_Encode1Bit(&cmd, frame);
    return true;
}

bool Encoder_BuildBank(void)
{
    pre_encoded_mask = 0U;

    uint8_t input_bits = enabled_input_bits();
    ModInput input;
    uint8_t raw_symbol = 0;

    if (input_bits > 0U) {
        if (SymbolBuf_HasData()) {
            if (!SymbolBuf_ReadBits(&raw_symbol, input_bits)) {
                return false;
            }
        } else {
            /* No symbol data yet; encoder runs with whatever input is available.
             * CW channels do not consume symbols and are always built. */
        }
    }
    make_mod_input(raw_symbol, input_bits, &input);

    for (uint8_t ch = 0; ch < DDS_CHANNEL_COUNT; ch++) {
        if (!mod_cfg[ch].enabled) continue;
        if (build_channel_frame(ch, &input, &pre_encoded[ch])) {
            pre_encoded_mask |= (1U << ch);
        }
    }

    return (pre_encoded_mask != 0U);
}

#if !AD9959_DOWNGRADE_MODE
/**
 * @brief  DMA mode: encode all active channels directly into the DMA
 *         target buffer (zero-copy, no intermediate DDS_EncodedFrame).
 *
 * For each enabled channel, computes the DDS_Command (same modulation logic
 * as Encoder_BuildBank) and writes the flat 14-byte register sequence
 * straight into TxBuf_GetIdle()->spi1[] in D2 SRAM.  Updates tx_bank_bytes.
 *
 * @return total bytes written, 0 if no channel produced a frame
 */
uint16_t Encoder_BuildBank_DMA(void)
{
    FrameBank *idle = TxBuf_GetIdle();
    uint16_t total = 0;

    uint8_t input_bits = enabled_input_bits();
    ModInput input;
    uint8_t raw_symbol = 0;

    if (input_bits > 0U) {
        if (SymbolBuf_HasData()) {
            if (!SymbolBuf_ReadBits(&raw_symbol, input_bits)) {
                return 0;
            }
        }
    }
    make_mod_input(raw_symbol, input_bits, &input);

    for (uint8_t ch = 0; ch < DDS_CHANNEL_COUNT; ch++) {
        if (!mod_cfg[ch].enabled) continue;

        DDS_Command cmd;
        if (!build_channel_command(ch, &input, &cmd)) {
            continue;
        }
        Encoder_Encode1Bit_Direct(&cmd, idle->spi1 + total);
        total += ENCODER_FRAME_FLAT_BYTES;
    }

    if (total < FRAME_BUF_SIZE) {
        memset(idle->spi1 + total, 0, FRAME_BUF_SIZE - total);
    }
    tx_bank_bytes = total;
    return total;
}
#endif /* !AD9959_DOWNGRADE_MODE */
