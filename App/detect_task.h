#ifndef __DETECT_TASK_H
#define __DETECT_TASK_H

#include <stdint.h>
#include <stdbool.h>
#include "main.h"

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
    /* DDS Power-Up Sequence — AD9959 datasheet timing (HSE=12M, SYSCLK=486.4MHz) */
    DDS_POWER_STABLE_TOE,     /**< 15ms: wait for 1.8V/3.3V rails + REFCLK to stabilize */
    DDS_RESET_HOLD_TOE,       /**<  2ms: Master Reset hold time (active low pulse width)    */
    DDS_RESET_RECOVERY_TOE,   /**<  5ms: wait after Master Reset release for register defaults */
    DDS_PLL_LOCK_TOE,         /**<  1ms: PLL lock time after FR1 write                     */

    DETECT_LIST_LENGTH,
};

void Detect_Init();
void Detect_Task();
void Detect_Hook(uint8_t toe);
uint8_t is_TOE_Overtime(uint8_t toe);

#endif