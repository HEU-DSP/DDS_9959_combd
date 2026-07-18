/**
 ******************************************************************************
 * @file    bsp_uart.h
 * @brief   BSP UART abstraction header
 ******************************************************************************
 */

#ifndef __BSP_UART_H__
#define __BSP_UART_H__

#include "main.h"

void BSP_UART4_Write(const uint8_t *data, uint16_t size);
void BSP_USART1_Write(const uint8_t *data, uint16_t size);
void BSP_USART1_WriteString(const char *str);
void BSP_USART10_Write(const uint8_t *data, uint16_t size);

#endif /* __BSP_UART_H__ */
