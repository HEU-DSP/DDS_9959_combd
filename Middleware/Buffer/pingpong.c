/**
 ******************************************************************************
 * @file    pingpong.c
 * @brief   Ping-Pong Double Buffer Implementation
 ******************************************************************************
 */

#include "pingpong.h"

void PingPong_Init(PingPong_Buffer *pp, uint8_t *bank_a, uint8_t *bank_b,
                   uint16_t bank_size)
{
    pp->bank_a   = bank_a;
    pp->bank_b   = bank_b;
    pp->bank_size = bank_size;
    pp->active   = 0;
    pp->dma_busy = false;
}

uint8_t* PingPong_GetWriteBuf(PingPong_Buffer *pp)
{
    /* Return the idle buffer (the one DMA is NOT reading) */
    return (pp->active == 0) ? pp->bank_b : pp->bank_a;
}

uint8_t* PingPong_GetReadBuf(PingPong_Buffer *pp)
{
    return (pp->active == 0) ? pp->bank_a : pp->bank_b;
}

void PingPong_Swap(PingPong_Buffer *pp)
{
    pp->active ^= 1;
}
