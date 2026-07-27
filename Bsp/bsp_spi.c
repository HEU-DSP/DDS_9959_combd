/**
 ******************************************************************************
 * @file    bsp_spi.c
 * @brief   BSP SPI abstraction — SPI1 + SPI3 for AD9959 dual-wire mode
 ******************************************************************************
 */

#include "bsp_spi.h"
#include "debug_state.h"

/* External HAL handles from CubeMX (main.c).
 * SPI3 removed in downgrade mode — hspi3 no longer exists. */
extern SPI_HandleTypeDef hspi1;

/* ================================================================
 * Basic SPI Transmit (blocking, for register init)
 * ================================================================ */

void BSP_SPI_Transmit(SPI_TypeDef *spi, const uint8_t *data, uint16_t size)
{
    if (spi != SPI1 || size == 0) return;
    HAL_SPI_Transmit(&hspi1, (uint8_t *)data, size, HAL_MAX_DELAY);
}

HAL_StatusTypeDef BSP_SPI_TransmitStatus(SPI_TypeDef *spi, const uint8_t *data,
                                         uint16_t size)
{
    if (spi != SPI1 || size == 0U) return HAL_ERROR;
    return HAL_SPI_Transmit(&hspi1, (uint8_t *)data, size, HAL_MAX_DELAY);
}

uint32_t BSP_SPI_GetError(SPI_TypeDef *spi)
{
    if (spi != SPI1) return 0U;
    return HAL_SPI_GetError(&hspi1);
}

/* ================================================================
 * DMA Transmit
 * ================================================================ */

HAL_StatusTypeDef BSP_SPI_DMA_Start(SPI_TypeDef *spi, const uint8_t *buf,
                                    uint16_t size)
{
    if (spi != SPI1 || size == 0U) return HAL_ERROR;
    HAL_StatusTypeDef status = HAL_SPI_Transmit_DMA(&hspi1, (uint8_t *)buf, size);
    ad9959_diag.spi1_start_count++;
    ad9959_diag.spi1_start_status = (uint32_t)status;
    ad9959_diag.spi1_hal_state = (uint32_t)HAL_SPI_GetState(&hspi1);
    ad9959_diag.spi1_error = HAL_SPI_GetError(&hspi1);
    return status;
}

void BSP_SPI_Abort(SPI_TypeDef *spi)
{
    if (spi == SPI1) HAL_SPI_Abort(&hspi1);
}

void BSP_SPI_Abort_IT(SPI_TypeDef *spi)
{
    if (spi == SPI1) HAL_SPI_Abort_IT(&hspi1);
}

HAL_StatusTypeDef BSP_SPI_GetState(SPI_TypeDef *spi)
{
    if (spi != SPI1) return HAL_ERROR;
    return HAL_SPI_GetState(&hspi1);
}

/* ================================================================
 * Convenience: Start both SPI DMA simultaneously
 * ================================================================ */

void BSP_SPI_Both_DMA_Start(const uint8_t *spi1_buf, const uint8_t *spi3_buf,
                            uint16_t size)
{
    (void)spi3_buf;  /* SPI3 removed in downgrade mode */
    ad9959_diag.spi1_buffer_addr = (uint32_t)spi1_buf;
    ad9959_diag.spi3_buffer_addr = 0U;

    if (HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY) {
        ad9959_diag.spi1_start_status = (uint32_t)HAL_BUSY;
        ad9959_diag.spi1_hal_state = (uint32_t)HAL_SPI_GetState(&hspi1);
        return;
    }

    (void)BSP_SPI_DMA_Start(SPI1, spi1_buf, size);
}

void BSP_SPI_Both_Abort(void)
{
    BSP_SPI_Abort(SPI1);
}

void BSP_SPI_Both_Abort_IT(void)
{
    BSP_SPI_Abort_IT(SPI1);
}
