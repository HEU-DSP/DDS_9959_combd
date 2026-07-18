/**
 ******************************************************************************
 * @file    packet.h
 * @brief   Packet Builder — Skeleton (Phase 3)
 ******************************************************************************
 */

#ifndef __PACKET_H__
#define __PACKET_H__

#include <stdint.h>

typedef struct {
    uint8_t *preamble;
    uint8_t *sync;
    uint8_t *payload;
    uint16_t payload_len;
    uint16_t crc;
} Packet;

void Packet_Build(Packet *pkt);
int  Packet_ToBits(const Packet *pkt, uint8_t *bits, int max_bits);

#endif /* __PACKET_H__ */
