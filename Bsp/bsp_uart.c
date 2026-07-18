/**
 ******************************************************************************
 * @file    bsp_uart.c
 * @brief   BSP UART abstraction
 ******************************************************************************
 */

#include "bsp_uart.h"

extern UART_HandleTypeDef huart4;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart10;

/* ---- UART4 (AD9959 auxiliary IO) ---- */

void BSP_UART4_Write(const uint8_t *data, uint16_t size)
{
    HAL_UART_Transmit(&huart4, (uint8_t *)data, size, HAL_MAX_DELAY);
}

/* ---- USART1 (debug console) ---- */

void BSP_USART1_Write(const uint8_t *data, uint16_t size)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)data, size, HAL_MAX_DELAY);
}

void BSP_USART1_WriteString(const char *str)
{
    while (*str) {
        BSP_USART1_Write((const uint8_t *)str, 1);
        str++;
    }
}

/* ---- USART10 (half-duplex, 595 serial data) ---- */

void BSP_USART10_Write(const uint8_t *data, uint16_t size)
{
    HAL_UART_Transmit(&huart10, (uint8_t *)data, size, HAL_MAX_DELAY);
}
