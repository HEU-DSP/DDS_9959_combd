/**
 ******************************************************************************
 * @file    bsp_dma.c
 * @brief   BSP DMA callback routing
 *
 * HAL_SPI_TxCpltCallback is overridden in main.c USER CODE.
 * This module provides a clean registration mechanism.
 ******************************************************************************
 */

#include "bsp_dma.h"

static BSP_DMA_Callback cb_spi1_done = NULL;
static BSP_DMA_Callback cb_spi3_done = NULL;

void BSP_DMA_SetCallback_Done(BSP_DMA_Callback cb1, BSP_DMA_Callback cb3)
{
    cb_spi1_done = cb1;
    cb_spi3_done = cb3;
}

/* Called from main.c HAL_SPI_TxCpltCallback */
void BSP_DMA_NotifyComplete(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1 && cb_spi1_done) {
        cb_spi1_done();
    } else if (hspi->Instance == SPI3 && cb_spi3_done) {
        cb_spi3_done();
    }
}
