#ifndef __DETECT_TASK_H
#define __DETECT_TASK_H

#include "controller.h"
#include <stdbool.h>

#define USER_GetTick HAL_GetTick

#ifdef _CMSIS_OS_H
#define USER_Delay_ms vTaskDelay
#else
#define USER_Delay_ms HAL_Delay
#endif

typedef struct 
{ 
    bool is_Lost;
    bool Overtime_Exit;
    float Overtime_ms;
    float new_time;
    float dt_ms;
    
}Detect_t;

enum errorlist
{
    ADC1_WATCHDOG1_TOE,
    ADC1_WATCHDOG2_TOE,
    ADC2_WATCHDOG1_TOE,
    CONNECTING_TO_CONNECTED_TOE,
    CAN_BIGCUP_RX_TOE,
    USART3_TX_TOE,
    USART3_RX_TOE,
    CONNECTED_UVLO_TIMEOUT_TOE,
    CAN_MIAO_RX_TOE,
    DETECT_LIST_LENGTH,

};

void Detect_Init();
void Detect_Task();
void Detect_Hook(uint8_t toe);
uint8_t is_TOE_Overtime(uint8_t toe);

#endif