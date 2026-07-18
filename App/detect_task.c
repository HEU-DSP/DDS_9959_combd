#include "detect_task.h"

static Detect_t Detect_List[DETECT_LIST_LENGTH + 1]={0};

void Detect_Init()
{
    float set_item[DETECT_LIST_LENGTH] = 
    {
        // ms, order matches enum errorlist in detect_task.h
        1000,   // ADC1_WATCHDOG1_TOE: VIN analog watchdog timeout
        1000,   // ADC1_WATCHDOG2_TOE: VOUT analog watchdog timeout
        1000,   // ADC2_WATCHDOG1_TOE: current analog watchdog timeout
        400,    // CONNECTING_TO_CONNECTED_TOE: delay before switching to Connected
        120,    // CAN_BIGCUP_RX_TOE: CAN receive timeout
        200,    // USART3_TX_TOE: USART3 transmit completion timeout
        400,    // USART3_RX_TOE: USART3 receive timeout
        5000,   // Connected_UVLO_timeout: timeout for undervoltage lockout in Connected state
        120,    // CAN_MIAO_RX_TOE: CAN receive timeout for Miao
    };

    for(uint8_t i = 0;i<DETECT_LIST_LENGTH;i++)
    {
        Detect_List[i].Overtime_ms = set_item[i];
        Detect_List[i].is_Lost = 1;
        Detect_List[i].Overtime_Exit = 1;
        Detect_List[i].dt_ms = 0.0f;
    }
}

void Detect_Task()
{
    Detect_List[DETECT_LIST_LENGTH].is_Lost = 0;
    Detect_List[DETECT_LIST_LENGTH].Overtime_Exit = 0;
    
    for(uint8_t i = 0;i<DETECT_LIST_LENGTH;i++)
    {
        Detect_List[i].dt_ms = (float)(USER_GetTick() - Detect_List[i].new_time);
        if(Detect_List[i].dt_ms > Detect_List[i].Overtime_ms)
        {
            if(!Detect_List[i].Overtime_Exit)
            {
                Detect_List[i].is_Lost = 1;
                Detect_List[i].Overtime_Exit = 1;
            }
            Detect_List[DETECT_LIST_LENGTH].is_Lost = 1;
            Detect_List[DETECT_LIST_LENGTH].Overtime_Exit = 1;
        }
        else
        {
            Detect_List[i].is_Lost = 0;
            Detect_List[i].Overtime_Exit = 0;
        }
    
    }
}

void Detect_Hook(uint8_t toe)
{
    Detect_List[toe].new_time = USER_GetTick();
    Detect_List[toe].is_Lost = 0;
}

uint8_t is_TOE_Overtime(uint8_t toe)
{
    return Detect_List[toe].Overtime_Exit;
}
