/**
 ******************************************************************************
 * @file    host_protocol.c
 * @brief   Binary host protocol parser, CRC, command dispatch.
 *
 * Runs entirely in main-loop context. Never calls SPI, TIM, or
 * DDS functions directly — only reads/writes pending_cfg and sym_buf.
 * APPLY/STOP are deferred to DDSControl_RequestApply/RequestStop.
 ******************************************************************************
 */

#include "host_protocol.h"
#include "dds_control.h"
#include "bsp_uart_rx.h"
#include "bsp_uart.h"
#include "symbol_buffer.h"
#include <string.h>

/* ---- External HAL handles ---- */
extern UART_HandleTypeDef huart1;

/* ---- CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, no XOR-out) ---- */

static uint16_t crc16_update(uint16_t crc, uint8_t byte)
{
    crc ^= (uint16_t)byte << 8;
    for (uint8_t b = 0U; b < 8U; b++) {
        if (crc & 0x8000U) {
            crc = (crc << 1) ^ 0x1021U;
        } else {
            crc <<= 1;
        }
    }
    return crc;
}

static uint16_t crc16_buf(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFU;
    while (len--) {
        crc = crc16_update(crc, *data++);
    }
    return crc;
}

/* ---- Staging config extern (storage in dds_control.c) ---- */
extern ChannelModConfig pending_cfg[DDS_CHANNEL_COUNT];
extern bool             pending_cfg_has_data;

/* ---- Frame buffer ---- */
static uint8_t  fbuf[HOST_RX_BUF_SIZE];
static uint16_t fbuf_idx;
static HostSyncState sync_state;
static uint8_t  frame_ver, frame_seq, frame_cmd;
static uint16_t frame_len;

/* ---- Reply buffer ---- */
static uint8_t  rbuf[HOST_FRAME_MAX];
static uint16_t rbuf_idx;

/* ================================================================
 * Reply helpers
 * ================================================================ */

static void reply_start(uint8_t seq, uint8_t cmd, uint16_t pay_len)
{
    rbuf_idx = 0U;
    rbuf[rbuf_idx++] = HOST_SOF1;
    rbuf[rbuf_idx++] = HOST_SOF2;
    rbuf[rbuf_idx++] = HOST_VER;
    rbuf[rbuf_idx++] = seq;
    rbuf[rbuf_idx++] = cmd;
    rbuf[rbuf_idx++] = (uint8_t)(pay_len & 0xFF);
    rbuf[rbuf_idx++] = (uint8_t)((pay_len >> 8) & 0xFF);
}

static void reply_add_u8(uint8_t v)
{
    if (rbuf_idx < HOST_FRAME_MAX) {
        rbuf[rbuf_idx++] = v;
    }
}

static void reply_add_u16(uint16_t v)
{
    reply_add_u8((uint8_t)(v & 0xFF));
    reply_add_u8((uint8_t)((v >> 8) & 0xFF));
}

static void reply_add_u32(uint32_t v)
{
    reply_add_u16((uint16_t)(v & 0xFFFFU));
    reply_add_u16((uint16_t)((v >> 16) & 0xFFFFU));
}

static void reply_send(uint8_t cmd)
{
    /* CRC over VER..payload */
    uint16_t crc = crc16_buf(&rbuf[2], rbuf_idx - 2);
    rbuf[rbuf_idx++] = (uint8_t)(crc & 0xFF);
    rbuf[rbuf_idx++] = (uint8_t)((crc >> 8) & 0xFF);
    HAL_UART_Transmit(&huart1, rbuf, rbuf_idx, HAL_MAX_DELAY);
    (void)cmd;
}

static void reply_ack(uint8_t seq, uint8_t cmd)
{
    reply_start(seq, cmd, 0U);
    reply_send(cmd);
}

static void reply_ack_u8(uint8_t seq, uint8_t cmd, uint8_t v)
{
    reply_start(seq, cmd, 1U);
    reply_add_u8(v);
    reply_send(cmd);
}

static void reply_nack(uint8_t seq, uint8_t cmd, uint8_t reason)
{
    reply_start(seq, cmd | HOST_FLAG_NACK, 1U);
    reply_add_u8(reason);
    reply_send(cmd);
}

/* ================================================================
 * Byte-level helpers
 * ================================================================ */

static uint16_t read_u16_le(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* ================================================================
 * Command handlers
 * ================================================================ */

static void handle_ping(uint8_t seq)
{
    reply_start(seq, HOST_CMD_PING, 4U);
    reply_add_u8(HOST_VER);
    reply_add_u8(0x01U);  /* FW_MAJOR */
    reply_add_u8(0x00U);  /* FW_MINOR */
    reply_add_u8(0x00U);
    reply_send(HOST_CMD_PING);
}

static void handle_get_status(uint8_t seq)
{
    reply_start(seq, HOST_CMD_GET_STATUS, DDS_CHANNEL_COUNT * 3U);
    for (uint8_t ch = 0; ch < DDS_CHANNEL_COUNT; ch++) {
        reply_add_u8(mod_cfg[ch].mode);
        reply_add_u8(mod_cfg[ch].enabled ? 1U : 0U);
        reply_add_u8(0x00U);
    }
    reply_send(HOST_CMD_GET_STATUS);
}

static void handle_get_diag(uint8_t seq)
{
    extern volatile AD9959_Diag ad9959_diag;
    extern DebugState ds;

    reply_start(seq, HOST_CMD_GET_DIAG, 28U);
    reply_add_u32(ad9959_diag.init_stage);
    reply_add_u32(ad9959_diag.frame_count);
    reply_add_u32((uint32_t)ds.frame_count);
    reply_add_u32(ad9959_diag.spi1_error);
    reply_add_u32(ad9959_diag.spi1_dma_error);
    reply_add_u32(ad9959_diag.tx_running);
    reply_add_u32(ad9959_diag.tx_stop_pending);
    reply_send(HOST_CMD_GET_DIAG);
}

static void handle_set_cw(uint8_t seq, const uint8_t *pay, uint16_t len)
{
    if (len < 7U) { reply_nack(seq, HOST_CMD_SET_CW, HOST_NACK_BAD_LEN); return; }
    uint8_t  ch   = pay[0];
    uint32_t freq = read_u32_le(&pay[1]);
    uint16_t asf  = read_u16_le(&pay[5]);
    if (ch >= DDS_CHANNEL_COUNT) { reply_nack(seq, HOST_CMD_SET_CW, HOST_NACK_INVALID_CH); return; }
    if (asf > DDSCALC_ASF_MAX)   { reply_nack(seq, HOST_CMD_SET_CW, HOST_NACK_BAD_PARAM); return; }

    pending_cfg[ch].mode    = CH_MODE_CW;
    pending_cfg[ch].enabled = true;
    pending_cfg[ch].cw.ftw  = DDSCalc_FTW(freq);
    pending_cfg[ch].cw.asf  = asf;
    pending_cfg_has_data = true;
    reply_ack(seq, HOST_CMD_SET_CW);
}

static void handle_set_fsk(uint8_t seq, const uint8_t *pay, uint16_t len)
{
    if (len < 11U) { reply_nack(seq, HOST_CMD_SET_FSK, HOST_NACK_BAD_LEN); return; }
    uint8_t  ch     = pay[0];
    uint32_t f_mark = read_u32_le(&pay[1]);
    uint32_t f_space= read_u32_le(&pay[5]);
    uint16_t asf    = read_u16_le(&pay[9]);
    if (ch >= DDS_CHANNEL_COUNT) { reply_nack(seq, HOST_CMD_SET_FSK, HOST_NACK_INVALID_CH); return; }

    pending_cfg[ch].mode           = CH_MODE_FSK;
    pending_cfg[ch].enabled        = true;
    pending_cfg[ch].fsk.ftw_mark   = DDSCalc_FTW(f_mark);
    pending_cfg[ch].fsk.ftw_space  = DDSCalc_FTW(f_space);
    pending_cfg[ch].fsk.asf        = asf;
    pending_cfg_has_data = true;
    reply_ack(seq, HOST_CMD_SET_FSK);
}

static void handle_set_ask(uint8_t seq, const uint8_t *pay, uint16_t len)
{
    if (len < 9U) { reply_nack(seq, HOST_CMD_SET_ASK, HOST_NACK_BAD_LEN); return; }
    uint8_t  ch     = pay[0];
    uint32_t freq   = read_u32_le(&pay[1]);
    uint16_t asf_on = read_u16_le(&pay[5]);
    uint16_t asf_off= read_u16_le(&pay[7]);
    if (ch >= DDS_CHANNEL_COUNT) { reply_nack(seq, HOST_CMD_SET_ASK, HOST_NACK_INVALID_CH); return; }

    pending_cfg[ch].mode        = CH_MODE_ASK;
    pending_cfg[ch].enabled     = true;
    pending_cfg[ch].ask.ftw     = DDSCalc_FTW(freq);
    pending_cfg[ch].ask.asf_on  = asf_on;
    pending_cfg[ch].ask.asf_off = asf_off;
    pending_cfg_has_data = true;
    reply_ack(seq, HOST_CMD_SET_ASK);
}

static void handle_set_bpsk(uint8_t seq, const uint8_t *pay, uint16_t len)
{
    if (len < 11U) { reply_nack(seq, HOST_CMD_SET_BPSK, HOST_NACK_BAD_LEN); return; }
    uint8_t  ch     = pay[0];
    uint32_t freq   = read_u32_le(&pay[1]);
    uint16_t asf    = read_u16_le(&pay[5]);
    uint16_t ph0    = read_u16_le(&pay[7]);
    uint16_t ph1    = read_u16_le(&pay[9]);
    if (ch >= DDS_CHANNEL_COUNT) { reply_nack(seq, HOST_CMD_SET_BPSK, HOST_NACK_INVALID_CH); return; }

    pending_cfg[ch].mode         = CH_MODE_BPSK;
    pending_cfg[ch].enabled      = true;
    pending_cfg[ch].bpsk.ftw     = DDSCalc_FTW(freq);
    pending_cfg[ch].bpsk.asf     = asf;
    pending_cfg[ch].bpsk.phase0  = ph0;
    pending_cfg[ch].bpsk.phase1  = ph1;
    pending_cfg_has_data = true;
    reply_ack(seq, HOST_CMD_SET_BPSK);
}

static void handle_set_qpsk(uint8_t seq, const uint8_t *pay, uint16_t len)
{
    if (len < 7U) { reply_nack(seq, HOST_CMD_SET_QPSK, HOST_NACK_BAD_LEN); return; }
    uint8_t  ch   = pay[0];
    uint32_t freq = read_u32_le(&pay[1]);
    uint16_t asf  = read_u16_le(&pay[5]);
    if (ch >= DDS_CHANNEL_COUNT) { reply_nack(seq, HOST_CMD_SET_QPSK, HOST_NACK_INVALID_CH); return; }

    pending_cfg[ch].mode              = CH_MODE_QPSK;
    pending_cfg[ch].enabled           = true;
    pending_cfg[ch].qpsk.ftw          = DDSCalc_FTW(freq);
    pending_cfg[ch].qpsk.asf          = asf;
    pending_cfg[ch].qpsk.phase_00     = 0x0800U;
    pending_cfg[ch].qpsk.phase_01     = 0x1800U;
    pending_cfg[ch].qpsk.phase_10     = 0x3800U;
    pending_cfg[ch].qpsk.phase_11     = 0x2800U;
    pending_cfg_has_data = true;
    reply_ack(seq, HOST_CMD_SET_QPSK);
}

static void handle_set_4fsk(uint8_t seq, const uint8_t *pay, uint16_t len)
{
    if (len < 19U) { reply_nack(seq, HOST_CMD_SET_4FSK, HOST_NACK_BAD_LEN); return; }
    uint8_t  ch  = pay[0];
    uint16_t asf = read_u16_le(&pay[1]);
    uint32_t f0  = read_u32_le(&pay[3]);
    uint32_t f1  = read_u32_le(&pay[7]);
    uint32_t f2  = read_u32_le(&pay[11]);
    uint32_t f3  = read_u32_le(&pay[15]);
    if (ch >= DDS_CHANNEL_COUNT) { reply_nack(seq, HOST_CMD_SET_4FSK, HOST_NACK_INVALID_CH); return; }

    pending_cfg[ch].mode        = CH_MODE_4FSK;
    pending_cfg[ch].enabled     = true;
    pending_cfg[ch].fsk4.asf    = asf;
    pending_cfg[ch].fsk4.ftw[0] = DDSCalc_FTW(f0);
    pending_cfg[ch].fsk4.ftw[1] = DDSCalc_FTW(f1);
    pending_cfg[ch].fsk4.ftw[2] = DDSCalc_FTW(f2);
    pending_cfg[ch].fsk4.ftw[3] = DDSCalc_FTW(f3);
    pending_cfg_has_data = true;
    reply_ack(seq, HOST_CMD_SET_4FSK);
}

static void handle_set_symbols(uint8_t seq, const uint8_t *pay, uint16_t len)
{
    if (len < 2U) { reply_nack(seq, HOST_CMD_SET_SYMBOLS, HOST_NACK_BAD_LEN); return; }
    uint16_t bit_count = read_u16_le(&pay[0]);
    if (bit_count == 0U || bit_count > SYM_BUF_CAPACITY || len < (2U + bit_count)) {
        reply_nack(seq, HOST_CMD_SET_SYMBOLS, HOST_NACK_BAD_LEN);
        return;
    }
    if (!SymbolBuf_IsFree()) {
        reply_nack(seq, HOST_CMD_SET_SYMBOLS, HOST_NACK_BUSY);
        return;
    }
    SymbolBuf_WriteBits(&pay[2], bit_count);
    reply_ack(seq, HOST_CMD_SET_SYMBOLS);
}

static void handle_apply(uint8_t seq)
{
    if (!pending_cfg_has_data) {
        reply_nack(seq, HOST_CMD_APPLY, HOST_NACK_NOT_READY);
        return;
    }
    if (DDSControl_IsBusy()) {
        reply_nack(seq, HOST_CMD_APPLY, HOST_NACK_BUSY);
        return;
    }
    DDSControl_RequestApply();
    /* Actual apply runs in DDSControl_Task() in the main loop.
     * Send ACK now — the caller can poll GET_STATUS to confirm. */
    uint8_t mask = 0U;
    for (uint8_t ch = 0; ch < DDS_CHANNEL_COUNT; ch++) {
        if (pending_cfg[ch].enabled) mask |= (1U << ch);
    }
    reply_ack_u8(seq, HOST_CMD_APPLY, mask);
}

static void handle_stop_output(uint8_t seq)
{
    if (DDSControl_IsBusy()) {
        reply_nack(seq, HOST_CMD_STOP_OUTPUT, HOST_NACK_BUSY);
        return;
    }
    DDSControl_RequestStop();
    reply_ack(seq, HOST_CMD_STOP_OUTPUT);
}

/* ================================================================
 * Frame dispatch
 * ================================================================ */

static void frame_dispatch(void)
{
    /* Compute CRC over VER..payload-before-CRC */
    uint16_t expected = crc16_buf(&fbuf[2], (uint16_t)(HOST_FRAME_MIN - 2U + frame_len));
    uint16_t received = (uint16_t)fbuf[HOST_FRAME_MIN - 2U + frame_len]
                      | ((uint16_t)fbuf[HOST_FRAME_MIN - 1U + frame_len] << 8);

    if (expected != received) {
        return;  /* silently drop bad-CRC frames */
    }

    const uint8_t *payload = &fbuf[HOST_FRAME_MIN - 2U];

    switch (frame_cmd) {
    case HOST_CMD_PING:         handle_ping(frame_seq);          break;
    case HOST_CMD_GET_STATUS:   handle_get_status(frame_seq);    break;
    case HOST_CMD_SET_CW:       handle_set_cw(frame_seq, payload, frame_len);      break;
    case HOST_CMD_SET_FSK:      handle_set_fsk(frame_seq, payload, frame_len);     break;
    case HOST_CMD_SET_ASK:      handle_set_ask(frame_seq, payload, frame_len);     break;
    case HOST_CMD_SET_BPSK:     handle_set_bpsk(frame_seq, payload, frame_len);    break;
    case HOST_CMD_SET_QPSK:     handle_set_qpsk(frame_seq, payload, frame_len);    break;
    case HOST_CMD_SET_4FSK:     handle_set_4fsk(frame_seq, payload, frame_len);    break;
    case HOST_CMD_SET_SYMBOLS:  handle_set_symbols(frame_seq, payload, frame_len); break;
    case HOST_CMD_APPLY:        handle_apply(frame_seq);         break;
    case HOST_CMD_STOP_OUTPUT:  handle_stop_output(frame_seq);   break;
    case HOST_CMD_GET_DIAG:     handle_get_diag(frame_seq);      break;
    default:
        reply_nack(frame_seq, frame_cmd, HOST_NACK_BAD_CMD);
        break;
    }
}

/* ================================================================
 * Frame parser state machine
 * ================================================================ */

static void parse_byte(uint8_t byte)
{
    switch (sync_state) {
    case HOST_SYNC_WAIT_SOF1:
        if (byte == HOST_SOF1) {
            sync_state = HOST_SYNC_WAIT_SOF2;
        }
        break;

    case HOST_SYNC_WAIT_SOF2:
        if (byte == HOST_SOF2) {
            fbuf_idx   = 2U;
            fbuf[0]    = HOST_SOF1;
            fbuf[1]    = HOST_SOF2;
            sync_state = HOST_SYNC_READ_HEADER;
        } else if (byte != HOST_SOF1) {
            sync_state = HOST_SYNC_WAIT_SOF1;
        }
        /* if byte==SOF1: stay in WAIT_SOF2 (SOF1 immediately after SOF1) */
        break;

    case HOST_SYNC_READ_HEADER:
        fbuf[fbuf_idx++] = byte;
        if (fbuf_idx >= HOST_FRAME_MIN) {
            frame_ver = fbuf[2];
            frame_seq = fbuf[3];
            frame_cmd = fbuf[4];
            frame_len = (uint16_t)fbuf[5] | ((uint16_t)fbuf[6] << 8);
            if (frame_len > HOST_PAYLOAD_MAX) {
                sync_state = HOST_SYNC_WAIT_SOF1;
                break;
            }
            if (frame_len == 0U) {
                sync_state = HOST_SYNC_READ_CRC1; /* skip payload */
            } else {
                sync_state = HOST_SYNC_READ_PAYLOAD;
            }
        }
        break;

    case HOST_SYNC_READ_PAYLOAD:
        fbuf[fbuf_idx++] = byte;
        if (fbuf_idx >= (HOST_FRAME_MIN + frame_len)) {
            sync_state = HOST_SYNC_READ_CRC1;
        }
        break;

    case HOST_SYNC_READ_CRC1:
        fbuf[fbuf_idx++] = byte;
        sync_state = HOST_SYNC_READ_CRC2;
        break;

    case HOST_SYNC_READ_CRC2:
        fbuf[fbuf_idx++] = byte;
        sync_state = HOST_SYNC_DONE;
        break;

    case HOST_SYNC_DONE:
    default:
        break;
    }
}

/* ================================================================
 * Public API
 * ================================================================ */

void HostProtocol_Init(void)
{
    sync_state = HOST_SYNC_WAIT_SOF1;
    fbuf_idx   = 0U;
    pending_cfg_has_data = false;
    memset(pending_cfg, 0, sizeof(pending_cfg));
}

void HostProtocol_Task(void)
{
    /* Consume all bytes from the RX ring buffer */
    while (BSP_UartRx_Available()) {
        uint8_t byte;
        if (!BSP_UartRx_ReadByte(&byte)) break;
        parse_byte(byte);

        if (sync_state == HOST_SYNC_DONE) {
            frame_dispatch();
            sync_state = HOST_SYNC_WAIT_SOF1;
            fbuf_idx   = 0U;
        }
    }
}
