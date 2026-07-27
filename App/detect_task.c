/**
 ******************************************************************************
 * @file    detect_task.c
 * @brief   Timeout detection module implementation
 ******************************************************************************
 */

#include "detect_task.h"

static DetectTask_TypeDef Detect_List[DETECT_LIST_LENGTH + 1] = {0};

void DetectTask_Init(void)
{
    float set_item[DETECT_LIST_LENGTH] =
    {
        /* ms — order matches enum DetectTask_EventList */
        1000,   /* DETECT_EVENT_0 */
        1000,   /* DETECT_EVENT_1 */
        1000,   /* DETECT_EVENT_2 */
        400,    /* DETECT_EVENT_3 */
        120,    /* DETECT_EVENT_4 */
        200,    /* DETECT_EVENT_5 */
        400,    /* DETECT_EVENT_6 */
        5000,   /* DETECT_EVENT_7 */
        120,    /* DETECT_EVENT_8 */
    };

    for (uint8_t i = 0; i < DETECT_LIST_LENGTH; i++)
    {
        Detect_List[i].overtime_ms = set_item[i];
        Detect_List[i].is_lost = true;
        Detect_List[i].overtime_exit = true;
        Detect_List[i].dt_ms = 0.0f;
    }
}

void DetectTask_Process(void)
{
    Detect_List[DETECT_LIST_LENGTH].is_lost = false;
    Detect_List[DETECT_LIST_LENGTH].overtime_exit = false;

    for (uint8_t i = 0; i < DETECT_LIST_LENGTH; i++)
    {
        Detect_List[i].dt_ms = (float)(USER_GetTick() - Detect_List[i].new_time);
        if (Detect_List[i].dt_ms > Detect_List[i].overtime_ms)
        {
            if (!Detect_List[i].overtime_exit)
            {
                Detect_List[i].is_lost = true;
                Detect_List[i].overtime_exit = true;
            }
            Detect_List[DETECT_LIST_LENGTH].is_lost = true;
            Detect_List[DETECT_LIST_LENGTH].overtime_exit = true;
        }
        else
        {
            Detect_List[i].is_lost = false;
            Detect_List[i].overtime_exit = false;
        }
    }
}

void DetectTask_Hook(uint8_t event)
{
    Detect_List[event].new_time = USER_GetTick();
    Detect_List[event].is_lost = false;
}

uint8_t DetectTask_IsOvertime(uint8_t event)
{
    return Detect_List[event].overtime_exit;
}
