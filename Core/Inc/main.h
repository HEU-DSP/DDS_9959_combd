/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define OCR_STCP_Pin GPIO_PIN_2
#define OCR_STCP_GPIO_Port GPIOE
#define OCR_DS_Pin GPIO_PIN_3
#define OCR_DS_GPIO_Port GPIOE
#define OCR_SHCP_Pin GPIO_PIN_4
#define OCR_SHCP_GPIO_Port GPIOE
#define OCR_NMR_Pin GPIO_PIN_5
#define OCR_NMR_GPIO_Port GPIOE
#define OCR_NOE_Pin GPIO_PIN_6
#define OCR_NOE_GPIO_Port GPIOE
#define SYNC_LPTIM3_TO_TIM8_ETR_Pin GPIO_PIN_0
#define SYNC_LPTIM3_TO_TIM8_ETR_GPIO_Port GPIOA
#define ANALOG_EXT_O1_Pin GPIO_PIN_4
#define ANALOG_EXT_O1_GPIO_Port GPIOC
#define ANALOG_EXT_1_Pin GPIO_PIN_0
#define ANALOG_EXT_1_GPIO_Port GPIOB
#define ANALOG_EXT_O2_Pin GPIO_PIN_7
#define ANALOG_EXT_O2_GPIO_Port GPIOE
#define ANALOG_EXT_2_Pin GPIO_PIN_9
#define ANALOG_EXT_2_GPIO_Port GPIOE
#define SYNC_9959_IO_UPDATE_Pin GPIO_PIN_6
#define SYNC_9959_IO_UPDATE_GPIO_Port GPIOC
#define SPI_9959_DIO3_Pin GPIO_PIN_7
#define SPI_9959_DIO3_GPIO_Port GPIOC
#define SPI_9959_CS_Pin GPIO_PIN_15
#define SPI_9959_CS_GPIO_Port GPIOA
#define REF_9959_CLK_Pin GPIO_PIN_12
#define REF_9959_CLK_GPIO_Port GPIOC
#define IO_9959_3_Pin GPIO_PIN_0
#define IO_9959_3_GPIO_Port GPIOD
#define IO_9959_2_Pin GPIO_PIN_1
#define IO_9959_2_GPIO_Port GPIOD
#define IO_9959_1_Pin GPIO_PIN_2
#define IO_9959_1_GPIO_Port GPIOD
#define IO_9959_0_Pin GPIO_PIN_3
#define IO_9959_0_GPIO_Port GPIOD
#define SPI_9959_DIO3_BP_Pin GPIO_PIN_4
#define SPI_9959_DIO3_BP_GPIO_Port GPIOD
#define IO_9959_DIO2_Pin GPIO_PIN_5
#define IO_9959_DIO2_GPIO_Port GPIOD
#define SPI_9959_DIO1_Pin GPIO_PIN_6
#define SPI_9959_DIO1_GPIO_Port GPIOD
#define SPI_9959_DIO0_Pin GPIO_PIN_7
#define SPI_9959_DIO0_GPIO_Port GPIOD
#define SPI_9959_SCLK_Pin GPIO_PIN_3
#define SPI_9959_SCLK_GPIO_Port GPIOB
#define SPI_9959_IO2_R_Pin GPIO_PIN_4
#define SPI_9959_IO2_R_GPIO_Port GPIOB
#define SPI_9959_CS_CAPTURE1_Pin GPIO_PIN_6
#define SPI_9959_CS_CAPTURE1_GPIO_Port GPIOB
#define SPI_9959_CS_CAPTURE2_Pin GPIO_PIN_7
#define SPI_9959_CS_CAPTURE2_GPIO_Port GPIOB
#define SYNC_9959_IO_UPDATE_BP_Pin GPIO_PIN_8
#define SYNC_9959_IO_UPDATE_BP_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
