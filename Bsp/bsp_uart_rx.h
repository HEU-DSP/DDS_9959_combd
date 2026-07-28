/**
 ******************************************************************************
 * @file    bsp_uart_rx.h
 * @brief   USART1 RX DMA + IDLE line detection + ring buffer
 *
 * USART1 (PA9=TX, PA10=RX, 115200 8N1) receives host protocol frames.
 * DMA1 Stream 0 in circular mode continuously captures bytes.
 * USART IDLE interrupt signals end-of-frame; ISR copies data into
 * a ring buffer for main-loop consumption.
 *
 * The ISR never touches SPI1, DDS config, or TIM8.
 ******************************************************************************
 */

#ifndef __BSP_UART_RX_H__
#define __BSP_UART_RX_H__

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

#define UART_RX_BUF_SIZE  256U
#define UART_RX_FRAME_MAX 128U

void BSP_UartRx_Init(UART_HandleTypeDef *huart);
void BSP_UartRx_IdleISR(void);
bool BSP_UartRx_Available(void);
bool BSP_UartRx_ReadByte(uint8_t *byte);
void BSP_UartRx_Flush(void);
uint16_t BSP_UartRx_Count(void);

#endif /* __BSP_UART_RX_H__ */
