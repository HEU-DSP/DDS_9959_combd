#ifndef __BSP_TIM_H__
#define __BSP_TIM_H__

#include "main.h"

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim);
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);

#endif