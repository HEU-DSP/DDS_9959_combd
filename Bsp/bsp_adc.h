#ifndef __BSP_ADC_H__
#define __BSP_ADC_H__

#include "main.h"
#include "adc.h"
#include "dac.h"

#define ADC1_DMA_MEMORY_DATAWITCH 16
#define ADC1_DMA_MEMORY_SIZE 2
#define ADC1_FILTER_COUNT 3
#define END_Charging_FER 0.30f
#define COMP_RATIO_DETECT_DEFAULT 350U

extern float ADC1_Rx_f[ADC1_DMA_MEMORY_SIZE];
extern float Current_f;
extern float EDVoltage_f_for_DAC;
extern float Current_f_f;
extern uint32_t Detect_CNT;
extern volatile uint16_t Comp_Ratio_detect;

void ADC_Device_Init(void);
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc);
void HAL_ADC_LevelOutOfWindowCallback(ADC_HandleTypeDef *hadc);
void HAL_ADCEx_LevelOutOfWindow2Callback(ADC_HandleTypeDef *hadc);

#endif
