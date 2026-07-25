/**
 ******************************************************************************
 * @file    bsp_spi.c
 * @brief   BSP SPI abstraction — SPI1 + SPI3 for AD9959 dual-wire mode
 ******************************************************************************
 */

#include "bsp_spi.h"
#include "debug_state.h"

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

HAL_StatusTypeDef BSP_SPI_TransmitStatus(SPI_TypeDef *spi, const uint8_t *data,
                                         uint16_t size)
{
    SPI_HandleTypeDef *hspi = (spi == SPI1) ? &hspi1 : &hspi3;
    if (size == 0U) return HAL_OK;
    return HAL_SPI_Transmit(hspi, (uint8_t *)data, size, HAL_MAX_DELAY);
}

uint32_t BSP_SPI_GetError(SPI_TypeDef *spi)
{
    SPI_HandleTypeDef *hspi = (spi == SPI1) ? &hspi1 : &hspi3;
    return HAL_SPI_GetError(hspi);
}

/* ================================================================
 * DMA Transmit
 * ================================================================ */

HAL_StatusTypeDef BSP_SPI_DMA_Start(SPI_TypeDef *spi, const uint8_t *buf,
                                    uint16_t size)
{
    SPI_HandleTypeDef *hspi = (spi == SPI1) ? &hspi1 : &hspi3;
    if (size == 0U) return HAL_ERROR;
    HAL_StatusTypeDef status = HAL_SPI_Transmit_DMA(hspi, (uint8_t *)buf, size);
    if (spi == SPI1) {
        ad9959_diag.spi1_start_count++;
        ad9959_diag.spi1_start_status = (uint32_t)status;
        ad9959_diag.spi1_hal_state = (uint32_t)HAL_SPI_GetState(hspi);
        ad9959_diag.spi1_error = HAL_SPI_GetError(hspi);
    } else {
        ad9959_diag.spi3_start_count++;
        ad9959_diag.spi3_start_status = (uint32_t)status;
        ad9959_diag.spi3_hal_state = (uint32_t)HAL_SPI_GetState(hspi);
        ad9959_diag.spi3_error = HAL_SPI_GetError(hspi);
    }
    return status;
}

void BSP_SPI_Abort(SPI_TypeDef *spi)
{
    SPI_HandleTypeDef *hspi = (spi == SPI1) ? &hspi1 : &hspi3;
    HAL_SPI_Abort(hspi);
}

void BSP_SPI_Abort_IT(SPI_TypeDef *spi)
{
    SPI_HandleTypeDef *hspi = (spi == SPI1) ? &hspi1 : &hspi3;
    HAL_SPI_Abort_IT(hspi);
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
    ad9959_diag.spi1_buffer_addr = (uint32_t)spi1_buf;
    ad9959_diag.spi3_buffer_addr = (uint32_t)spi3_buf;

    /* Never let one serial lane advance alone.  A partial two-bit AD9959
     * frame is worse than a skipped sample because it corrupts the command. */
    if ((HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY) ||
        (HAL_SPI_GetState(&hspi3) != HAL_SPI_STATE_READY)) {
        ad9959_diag.spi1_start_status = (uint32_t)HAL_BUSY;
        ad9959_diag.spi3_start_status = (uint32_t)HAL_BUSY;
        ad9959_diag.spi1_hal_state = (uint32_t)HAL_SPI_GetState(&hspi1);
        ad9959_diag.spi3_hal_state = (uint32_t)HAL_SPI_GetState(&hspi3);
        return;
    }

    if (BSP_SPI_DMA_Start(SPI1, spi1_buf, size) != HAL_OK) {
        return;
    }
    if (BSP_SPI_DMA_Start(SPI3, spi3_buf, size) != HAL_OK) {
        BSP_SPI_Abort_IT(SPI1);
    }
}

void BSP_SPI_Both_Abort(void)
{
    BSP_SPI_Abort(SPI1);
    BSP_SPI_Abort(SPI3);
}

void BSP_SPI_Both_Abort_IT(void)
{
    BSP_SPI_Abort_IT(SPI1);
    BSP_SPI_Abort_IT(SPI3);
}
