#include "bsp_tim.h"

#include "ChargeTx.h"
#include "detect_task.h"
#include "RxData.h"

static uint16_t ic1[256] = {0};// 20->256 修复进捕获HardFault问题
static uint16_t ic2[256] = {0};
static uint8_t channel1_count = 0;
static uint8_t channel2_count = 0;
uint32_t Detect_CNT;

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if (TIM1 != htim->Instance) return;

    

    Detect_Hook(TIM_IC_IT_TOE);
    if (HAL_TIM_ACTIVE_CHANNEL_1 == htim->Channel) 
    {
        ic1[channel1_count] = TIM1->CCR1;
        channel1_count++;
    } 
    else if (HAL_TIM_ACTIVE_CHANNEL_2 == htim->Channel) 
    {
        ic2[channel2_count] = TIM1->CCR2;
        channel2_count++;
        Detect_CNT = TIM5->CCR1;
    }
  
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    
    if(htim->Instance == TIM16)
    {
        // 10kHz
        Detect_Task();
        ChargeTx_Task();
    }
    else if(htim->Instance == TIM20)
    {
        // 1kHz
        
        if(is_TOE_Overtime(TIM_IC_IT_TOE))
        {
            if(channel1_count == FRAME_RISING_EDGE_NUM && channel2_count == FRAME_FALLING_EDGE_NUM)
                Data_Handle(ic1,ic2);
            channel1_count = channel2_count = 0;
        }

    }
    
}
