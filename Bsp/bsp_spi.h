/**
 ******************************************************************************
 * @file    bsp_spi.h
 * @brief   BSP SPI abstraction header
 ******************************************************************************
 */

#ifndef __BSP_SPI_H__
#define __BSP_SPI_H__

#include "main.h"

/* ---- Basic (blocking) ---- */
void BSP_SPI_Transmit(SPI_TypeDef *spi, const uint8_t *data, uint16_t size);
HAL_StatusTypeDef BSP_SPI_TransmitStatus(SPI_TypeDef *spi, const uint8_t *data,
                                         uint16_t size);
uint32_t BSP_SPI_GetError(SPI_TypeDef *spi);

/* ---- DMA ---- */
HAL_StatusTypeDef BSP_SPI_DMA_Start(SPI_TypeDef *spi, const uint8_t *buf,
                                    uint16_t size);
void BSP_SPI_Abort(SPI_TypeDef *spi);
void BSP_SPI_Abort_IT(SPI_TypeDef *spi);
HAL_StatusTypeDef BSP_SPI_GetState(SPI_TypeDef *spi);

/* ---- Convenience: both SPI1 + SPI3 in parallel ---- */
void BSP_SPI_Both_DMA_Start(const uint8_t *spi1_buf, const uint8_t *spi3_buf,
                            uint16_t size);
void BSP_SPI_Both_Abort(void);
void BSP_SPI_Both_Abort_IT(void);


#endif /* __BSP_SPI_H__ */
