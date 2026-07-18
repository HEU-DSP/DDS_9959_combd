/**
 ******************************************************************************
 * @file    pingpong.h
 * @brief   Ping-Pong Double Buffer Manager
 *
 * Manages two independent buffers for continuous DMA streaming.
 * CPU fills one while DMA reads the other.
 ******************************************************************************
 */

#ifndef __PINGPONG_H__
#define __PINGPONG_H__

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t  *bank_a;        /* Bank A buffer (SPI lane)     */
    uint8_t  *bank_b;        /* Bank B buffer                */
    uint16_t  bank_size;     /* Bytes per bank               */
    uint8_t   active;        /* 0=Bank A (DMA reading), 1=B  */
    volatile bool dma_busy;  /* DMA transfer in progress     */
} PingPong_Buffer;

/**
 * @brief  Initialize Ping-Pong buffer
 */
void PingPong_Init(PingPong_Buffer *pp, uint8_t *bank_a, uint8_t *bank_b,
                   uint16_t bank_size);

/**
 * @brief  Get pointer to the idle buffer for CPU to write
 */
uint8_t* PingPong_GetWriteBuf(PingPong_Buffer *pp);

/**
 * @brief  Get pointer to the active buffer (DMA is reading this)
 */
uint8_t* PingPong_GetReadBuf(PingPong_Buffer *pp);

/**
 * @brief  Swap active bank (call from DMA complete ISR)
 */
void PingPong_Swap(PingPong_Buffer *pp);

#endif /* __PINGPONG_H__ */
