/**
 ******************************************************************************
 * @file    bsp_gpio.h
 * @brief   BSP GPIO abstraction header
 ******************************************************************************
 */

#ifndef __BSP_GPIO_H__
#define __BSP_GPIO_H__

#include "main.h"

/* ---- 74HC595 Control ---- */
void BSP_GPIO_595_STCP_Set(void);   /* Storage Clock (latch) high */
void BSP_GPIO_595_STCP_Clr(void);
void BSP_GPIO_595_SHCP_Set(void);   /* Shift Clock high */
void BSP_GPIO_595_SHCP_Clr(void);
void BSP_GPIO_595_MR_Set(void);     /* Master Reset release (high) */
void BSP_GPIO_595_MR_Clr(void);     /* Master Reset assert (low) */
void BSP_GPIO_595_OE_Clr(void);     /* Output Enable (low = active) */
void BSP_GPIO_595_OE_Set(void);     /* Output Disable (high = Hi-Z) */

/* ---- AD9959 DIO2 (PD5, reserved) ---- */
void BSP_GPIO_DIO2_Set(void);
void BSP_GPIO_DIO2_Clr(void);
GPIO_PinState BSP_GPIO_DIO2_Get(void);

/* ---- IO_9959 General-purpose (PD0–PD3) ---- */
void BSP_GPIO_IO9959_Set(uint8_t idx);
void BSP_GPIO_IO9959_Clr(uint8_t idx);

#endif /* __BSP_GPIO_H__ */
