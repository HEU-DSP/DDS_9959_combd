/**
 ******************************************************************************
 * @file    bsp_spi.c
 * @brief   BSP SPI abstraction — SPI1 + SPI3 for AD9959 dual-wire mode
 ******************************************************************************
 */

#include "bsp_spi.h"

/* External HAL handles from CubeMX (main.c) */
extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi3;

/* ================================================================
 * Basic SPI Transmit (blocking, for register init)
 * ================================================================ */

void BSP_SPI_Transmit(SPI_TypeDef *spi, const uint8_t *data, uint16_t size)
{
    SPI_HandleTypeDef *hspi = (spi == SPI1) ? &hspi1 : &hspi3;
    if (size == 0) return;
    HAL_SPI_Transmit(hspi, (uint8_t *)data, size, HAL_MAX_DELAY);
}

/* ================================================================
 * DMA Transmit
 * ================================================================ */

void BSP_SPI_DMA_Start(SPI_TypeDef *spi, const uint8_t *buf, uint16_t size)
{
    SPI_HandleTypeDef *hspi = (spi == SPI1) ? &hspi1 : &hspi3;
    if (size == 0) return;
    HAL_SPI_Transmit_DMA(hspi, (uint8_t *)buf, size);
}

void BSP_SPI_Abort(SPI_TypeDef *spi)
{
    SPI_HandleTypeDef *hspi = (spi == SPI1) ? &hspi1 : &hspi3;
    HAL_SPI_Abort(hspi);
}

HAL_StatusTypeDef BSP_SPI_GetState(SPI_TypeDef *spi)
{
    SPI_HandleTypeDef *hspi = (spi == SPI1) ? &hspi1 : &hspi3;
    return HAL_SPI_GetState(hspi);
}

/* ================================================================
 * Convenience: Start both SPI DMA simultaneously
 * ================================================================ */

void BSP_SPI_Both_DMA_Start(const uint8_t *spi1_buf, const uint8_t *spi3_buf,
                            uint16_t size)
{
    BSP_SPI_DMA_Start(SPI1, spi1_buf, size);
    BSP_SPI_DMA_Start(SPI3, spi3_buf, size);
}

void BSP_SPI_Both_Abort(void)
{
    BSP_SPI_Abort(SPI1);
    BSP_SPI_Abort(SPI3);
}
