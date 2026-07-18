/**
 ******************************************************************************
 * @file    bsp_gpio.c
 * @brief   BSP GPIO abstraction — 74HC595 control + DIO2 + IO_9959
 ******************************************************************************
 */

#include "bsp_gpio.h"

/* ================================================================
 * 74HC595 Control Signals
 * ================================================================ */

void BSP_GPIO_595_STCP_Set(void)
{
    HAL_GPIO_WritePin(OCR_STCP_GPIO_Port, OCR_STCP_Pin, GPIO_PIN_SET);
}

void BSP_GPIO_595_STCP_Clr(void)
{
    HAL_GPIO_WritePin(OCR_STCP_GPIO_Port, OCR_STCP_Pin, GPIO_PIN_RESET);
}

void BSP_GPIO_595_SHCP_Set(void)
{
    HAL_GPIO_WritePin(OCR_SHCP_GPIO_Port, OCR_SHCP_Pin, GPIO_PIN_SET);
}

void BSP_GPIO_595_SHCP_Clr(void)
{
    HAL_GPIO_WritePin(OCR_SHCP_GPIO_Port, OCR_SHCP_Pin, GPIO_PIN_RESET);
}

void BSP_GPIO_595_MR_Set(void)   /* MR high = not reset */
{
    HAL_GPIO_WritePin(OCR_NMR_GPIO_Port, OCR_NMR_Pin, GPIO_PIN_SET);
}

void BSP_GPIO_595_MR_Clr(void)   /* MR low = reset shift registers */
{
    HAL_GPIO_WritePin(OCR_NMR_GPIO_Port, OCR_NMR_Pin, GPIO_PIN_RESET);
}

void BSP_GPIO_595_OE_Clr(void)   /* OE low = outputs enabled */
{
    HAL_GPIO_WritePin(OCR_NOE_GPIO_Port, OCR_NOE_Pin, GPIO_PIN_RESET);
}

void BSP_GPIO_595_OE_Set(void)   /* OE high = outputs disabled (Hi-Z) */
{
    HAL_GPIO_WritePin(OCR_NOE_GPIO_Port, OCR_NOE_Pin, GPIO_PIN_SET);
}

/* ================================================================
 * AD9959 DIO2 (PD5) — Reserved in 2-wire mode
 * ================================================================ */

void BSP_GPIO_DIO2_Set(void)
{
    HAL_GPIO_WritePin(IO_9959_DIO2_GPIO_Port, IO_9959_DIO2_Pin, GPIO_PIN_SET);
}

void BSP_GPIO_DIO2_Clr(void)
{
    HAL_GPIO_WritePin(IO_9959_DIO2_GPIO_Port, IO_9959_DIO2_Pin, GPIO_PIN_RESET);
}

GPIO_PinState BSP_GPIO_DIO2_Get(void)
{
    return HAL_GPIO_ReadPin(IO_9959_DIO2_GPIO_Port, IO_9959_DIO2_Pin);
}

/* ================================================================
 * IO_9959 General-purpose pins (PD0–PD3)
 * ================================================================ */

void BSP_GPIO_IO9959_Set(uint8_t idx)
{
    switch (idx) {
    case 3: HAL_GPIO_WritePin(IO_9959_3_GPIO_Port, IO_9959_3_Pin, GPIO_PIN_SET); break;
    case 2: HAL_GPIO_WritePin(IO_9959_2_GPIO_Port, IO_9959_2_Pin, GPIO_PIN_SET); break;
    case 1: HAL_GPIO_WritePin(IO_9959_1_GPIO_Port, IO_9959_1_Pin, GPIO_PIN_SET); break;
    case 0: HAL_GPIO_WritePin(IO_9959_0_GPIO_Port, IO_9959_0_Pin, GPIO_PIN_SET); break;
    default: break;
    }
}

void BSP_GPIO_IO9959_Clr(uint8_t idx)
{
    switch (idx) {
    case 3: HAL_GPIO_WritePin(IO_9959_3_GPIO_Port, IO_9959_3_Pin, GPIO_PIN_RESET); break;
    case 2: HAL_GPIO_WritePin(IO_9959_2_GPIO_Port, IO_9959_2_Pin, GPIO_PIN_RESET); break;
    case 1: HAL_GPIO_WritePin(IO_9959_1_GPIO_Port, IO_9959_1_Pin, GPIO_PIN_RESET); break;
    case 0: HAL_GPIO_WritePin(IO_9959_0_GPIO_Port, IO_9959_0_Pin, GPIO_PIN_RESET); break;
    default: break;
    }
}
