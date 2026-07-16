#include "bsp_hrtim.h"

uint32_t hrtim_debug = 0;

uint16_t repetition_count = 0;
uint16_t register_count = 0; 
uint16_t compare1_count = 0;

void HRTIM_Device_Init(void)
{
    repetition_count = 0;
    compare1_count = 0;

    // 使能HRTIM1的输出通道TA1、TB1、TF1、TF2   
    // 使能HRTIM1的主定时器
    while (HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TA1 | HRTIM_OUTPUT_TB1 | HRTIM_OUTPUT_TF1 | HRTIM_OUTPUT_TF2) != HAL_OK)
    {
        hrtim_debug++;
    }
    while (HAL_HRTIM_WaveformCountStart(&hhrtim1,HRTIM_TIMERID_MASTER | HRTIM_TIMERID_TIMER_A | HRTIM_TIMERID_TIMER_B | HRTIM_TIMERID_TIMER_F) != HAL_OK)
    {
        hrtim_debug++;
    }

}
