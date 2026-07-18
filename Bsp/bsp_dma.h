/**
 ******************************************************************************
 * @file    bsp_dma.h
 * @brief   BSP DMA callback management
 *
 * Actual DMA transfers use HAL_SPI_Transmit_DMA().
 * This module only routes HAL SPI DMA callbacks to user handlers.
 ******************************************************************************
 */

#ifndef __BSP_DMA_H__
#define __BSP_DMA_H__

#include "main.h"

typedef void (*BSP_DMA_Callback)(void);

/**
 * @brief  Register callbacks for SPI1 and SPI3 DMA completion
 */
void BSP_DMA_SetCallback_Done(BSP_DMA_Callback cb_spi1,
                              BSP_DMA_Callback cb_spi3);

/**
 * @brief  Dispatch from HAL_SPI_TxCpltCallback to registered callbacks
 */
void BSP_DMA_NotifyComplete(SPI_HandleTypeDef *hspi);

#endif /* __BSP_DMA_H__ */
