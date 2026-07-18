/**
 ******************************************************************************
 * @file    ring_buffer.h
 * @brief   Generic Ring Buffer Template
 ******************************************************************************
 */

#ifndef __RING_BUFFER_H__
#define __RING_BUFFER_H__

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t  *buffer;
    uint16_t  size;
    uint16_t  head;
    uint16_t  tail;
    uint16_t  count;
} RingBuffer;

void    RingBuf_Init(RingBuffer *rb, uint8_t *buf, uint16_t size);
bool    RingBuf_Put(RingBuffer *rb, uint8_t byte);
bool    RingBuf_Get(RingBuffer *rb, uint8_t *byte);
bool    RingBuf_IsEmpty(RingBuffer *rb);
bool    RingBuf_IsFull(RingBuffer *rb);
uint16_t RingBuf_Count(RingBuffer *rb);

#endif /* __RING_BUFFER_H__ */
