#include "bsp_adc.h"

#include "detect_task.h"
#include "controller.h"
#include "ChargeTx.h"

#if ADC1_DMA_MEMORY_DATAWITCH == 8
    uint8_t ADC1_Rx[ADC1_DMA_MEMORY_SIZE];
#elif ADC1_DMA_MEMORY_DATAWITCH == 16
    uint16_t ADC1_Rx[ADC1_DMA_MEMORY_SIZE];
#elif ADC1_DMA_MEMORY_DATAWITCH == 32
    uint32_t ADC1_Rx[ADC1_DMA_MEMORY_SIZE];
#endif

#define ADC_SAMPLING_FREQUENCY (170.0e6/64.0/60.0)
#define ADC1_RATIO (2.9838f / 2048.0f)
#define ADC1_OFFSET (-26.335f)
#define ADC_CURRENT_RATIO 15.9420f

First_Order_Filter_t ADC1_Filter[ADC1_FILTER_COUNT];
float ADC1_Rx_f[ADC1_DMA_MEMORY_SIZE] = {0};
float Current_f;
float Current_f_f;
float EDVoltage_f_for_DAC;
volatile uint16_t Comp_Ratio_detect = COMP_RATIO_DETECT_DEFAULT;

static void Change_ADC_AWD_Threshold(uint32_t *ADCx_TRx,uint16_t low_threshold,uint16_t high_threshold);

void ADC_Device_Init(void)
{
    // 滤波初始化
    First_Order_Filter_Init(&ADC1_Filter[0],1.0f/ADC_SAMPLING_FREQUENCY,30);// 电流检测，value*24 = power
    First_Order_Filter_Init(&ADC1_Filter[1],1.0f/ADC_SAMPLING_FREQUENCY,3);// 传给DAC
    First_Order_Filter_Init(&ADC1_Filter[2],1.0f/ADC_SAMPLING_FREQUENCY,3);// 用于检测充电结束

    // 开启ADC1
    while(HAL_ADCEx_Calibration_Start(&hadc1,ADC_SINGLE_ENDED | ADC_DIFFERENTIAL_ENDED) != HAL_OK)
    {
    }
    
    while(HAL_ADC_Start_DMA(&hadc1,ADC1_Rx,ADC1_DMA_MEMORY_SIZE) != HAL_OK)
    {
    }
    
    // uint16_t high_watch1 = -(6.0/ADC_CURRENT_RATIO - ADC1_Rx[0])/ADC1_RATIO + 2048;
    uint16_t high_watch1 = 4095;
    uint16_t low_watch1 = 0;
    uint16_t high_watch2 = 255;
    uint16_t low_watch2 = 0;

    Change_ADC_AWD_Threshold(&ADC1->TR1,low_watch1,high_watch1);    //  电流异物等阈值
    Change_ADC_AWD_Threshold(&ADC1->TR2,low_watch2,high_watch2);    //  电流未连接阈值

    ADC1->IER |= ADC_IT_EOS|ADC_IT_AWD1|ADC_IT_AWD2;
    ADC1->TR2 = 0;
    ADC1->AWD2CR = 1 << 1;

}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if(hadc->Instance == ADC1)
    {    
        static float Current;
        Current = (2048 - ADC1_Rx[0]) * ADC1_RATIO * ADC_CURRENT_RATIO + ADC1_OFFSET;
        
        Current_f = First_Order_Filter_Calculate(&ADC1_Filter[0], Current);
        EDVoltage_f_for_DAC = First_Order_Filter_Calculate(&ADC1_Filter[1], (float)ADC1_Rx[1] * 2.97921f / 4096.0f);
        Current_f_f = First_Order_Filter_Calculate(&ADC1_Filter[2], Current);

        if(tx_state == STAY_BY)
        {
            HAL_DAC_SetValue(&hdac1,DAC_CHANNEL_1,DAC_ALIGN_12B_R,Comp_Ratio_detect);
            HAL_DAC_SetValue(&hdac1,DAC_CHANNEL_2,DAC_ALIGN_12B_R,Comp_Ratio_detect);
        }
        else
        {
            volatile static float Comp_Ratio_comm = 0.80f;
            HAL_DAC_SetValue(&hdac1,DAC_CHANNEL_1,DAC_ALIGN_12B_R,Comp_Ratio_comm * EDVoltage_f_for_DAC / 2.97921f * 4096.0f);
            HAL_DAC_SetValue(&hdac1,DAC_CHANNEL_2,DAC_ALIGN_12B_R,Comp_Ratio_comm * EDVoltage_f_for_DAC / 2.97921f * 4096.0f);      
        }


        //if(Current < 0.6f*Current_f && tx_state == HIGH_POWER && Current_f > 0.4f)
        // if(Current_f > 4.2f)
        // {
        //     last_tx_state = tx_state;
        //     tx_state = ERR;
        //     Detect_Hook(CURRENT_FALLING_TOE);
        // }
        if((tx_state == HIGH_POWER && Current_f < END_Charging_FER * Current_f_f) && Current_f_f > 0.4f)
        {
            last_tx_state = tx_state;
            tx_state = ERR;
            Detect_Hook(CURRENT_FALLING_TOE);
        }
    }
}

void HAL_ADC_LevelOutOfWindowCallback(ADC_HandleTypeDef *hadc)
{
    if(hadc->Instance == ADC1)
    {
        Detect_Hook(ADC1_WATCHDOG1_TOE);
    }
}

void HAL_ADCEx_LevelOutOfWindow2Callback(ADC_HandleTypeDef *hadc)
{
    if(hadc->Instance == ADC1)
    {
        Detect_Hook(ADC1_WATCHDOG2_TOE);
    }
}

static void Change_ADC_AWD_Threshold(uint32_t *ADCx_TRx,uint16_t low_threshold,uint16_t high_threshold)
{
    *ADCx_TRx = (high_threshold << 16) | low_threshold;
}
