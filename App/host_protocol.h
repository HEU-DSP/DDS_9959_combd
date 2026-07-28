/**
 ******************************************************************************
 * @file    host_protocol.h
 * @brief   Binary host protocol: frame format, CRC, command dispatch
 *
 * Frame: SOF1 SOF2 VER SEQ CMD LEN_L LEN_H PAYLOAD CRC16_L CRC16_H
 *   0xA5 0x5A 0x01 ...
 * CRC-16/CCITT-FALSE over VER..PAYLOAD (excludes SOF bytes).
 *
 * The parser runs in main-loop context (HostProtocol_Task).
 * It never calls SPI, TIM, or DDS functions directly.
 * It enqueues replies via HostProtocol_Reply*, which uses
 * blocking HAL_UART_Transmit.
 ******************************************************************************
 */

#ifndef __HOST_PROTOCOL_H__
#define __HOST_PROTOCOL_H__

#include "main.h"
#include "mod_config.h"
#include "dds_calc.h"
#include "debug_state.h"
#include <stdint.h>
#include <stdbool.h>

/* ---- Protocol constants ---- */
#define HOST_SOF1          0xA5U
#define HOST_SOF2          0x5AU
#define HOST_VER           0x01U
#define HOST_HEADER_SIZE   7U      /* SOF1 SOF2 VER SEQ CMD LEN_L LEN_H */
#define HOST_CRC_SIZE      2U
#define HOST_FRAME_MIN     (HOST_HEADER_SIZE + HOST_CRC_SIZE)
#define HOST_FRAME_MAX     (HOST_FRAME_MIN + HOST_PAYLOAD_MAX)
#define HOST_PAYLOAD_MAX   64U
#define HOST_RX_BUF_SIZE   128U

/* ---- Command IDs ---- */
#define HOST_CMD_PING          0x00U
#define HOST_CMD_GET_STATUS    0x01U
#define HOST_CMD_SET_CW        0x02U
#define HOST_CMD_SET_FSK       0x03U
#define HOST_CMD_SET_ASK       0x04U
#define HOST_CMD_SET_BPSK      0x05U
#define HOST_CMD_SET_QPSK      0x06U
#define HOST_CMD_SET_4FSK      0x07U
#define HOST_CMD_SET_SYMBOLS   0x08U
#define HOST_CMD_APPLY         0x09U
#define HOST_CMD_STOP_OUTPUT   0x0AU
#define HOST_CMD_GET_DIAG      0x0BU

#define HOST_FLAG_NACK         0x80U

/* ---- NACK reason codes ---- */
#define HOST_NACK_OK              0x00U
#define HOST_NACK_BAD_CRC         0x01U
#define HOST_NACK_BAD_LEN         0x02U
#define HOST_NACK_BAD_CMD         0x03U
#define HOST_NACK_BAD_PARAM       0x04U
#define HOST_NACK_INVALID_CH      0x05U
#define HOST_NACK_BUSY            0x06U
#define HOST_NACK_NOT_READY       0x07U

/* ---- SYNC state machine ---- */
typedef enum {
    HOST_SYNC_WAIT_SOF1 = 0,
    HOST_SYNC_WAIT_SOF2,
    HOST_SYNC_READ_HEADER,
    HOST_SYNC_READ_PAYLOAD,
    HOST_SYNC_READ_CRC1,
    HOST_SYNC_READ_CRC2,
    HOST_SYNC_DONE,
} HostSyncState;

/*
 * Ozone-friendly host/parameter snapshot.
 * `active[]` is the configuration currently used by the DDS stream;
 * `pending[]` is the next complete four-channel configuration awaiting APPLY.
 * This object is diagnostic only and is never read by the timing chain.
 */
typedef struct {
    uint32_t rx_frame_count;
    uint32_t valid_frame_count;
    uint32_t crc_error_count;
    uint32_t reply_count;
    uint32_t nack_count;

    uint8_t  parser_state;
    uint8_t  last_rx_ver;
    uint8_t  last_rx_seq;
    uint8_t  last_rx_cmd;
    uint16_t last_rx_len;
    uint16_t last_crc_expected;
    uint16_t last_crc_received;

    uint8_t  last_reply_cmd;
    uint8_t  last_nack_reason;
    uint16_t last_reply_len;
    uint32_t last_tx_hal_status;

    uint16_t rx_ring_count;
    uint16_t symbol_count;
    uint8_t  symbol_ready;
    uint8_t  symbol_free;
    uint8_t  symbol_bits_per_sym;
    uint8_t  pending_cfg_has_data;

    ChannelModConfig active[DDS_CHANNEL_COUNT];
    ChannelModConfig pending[DDS_CHANNEL_COUNT];
} HostProtocolDebug;

extern volatile HostProtocolDebug host_protocol_debug;

/* ---- API ---- */

/**
 * @brief  Initialize the protocol parser. Must be called once after
 *         UART and DMA RX init.
 */
void HostProtocol_Init(void);

/**
 * @brief  Poll in main loop. Reads bytes from the RX ring buffer,
 *         assembles frames, dispatches commands. Sends replies
 *         via blocking UART TX (only from main-loop context).
 */
void HostProtocol_Task(void);

#endif /* __HOST_PROTOCOL_H__ */
