/**
 ******************************************************************************
 * @file    detect_task.h
 * @brief   Timeout detection module — generic event-loss monitor
 *
 * Monitors a list of events; each event has a configurable timeout.
 * Call DetectTask_Hook() to reset an event's timer when it fires.
 * DetectTask_Process() checks all events and flags any that have timed out.
 ******************************************************************************
 */

#ifndef __DETECT_TASK_H__
#define __DETECT_TASK_H__

#include <stdint.h>
#include <stdbool.h>

#define USER_GetTick HAL_GetTick

typedef struct
{
    bool  is_lost;
    bool  overtime_exit;
    float overtime_ms;
    float new_time;
    float dt_ms;
} DetectTask_TypeDef;

enum DetectTask_EventList
{
    DETECT_EVENT_0,
    DETECT_EVENT_1,
    DETECT_EVENT_2,
    DETECT_EVENT_3,
    DETECT_EVENT_4,
    DETECT_EVENT_5,
    DETECT_EVENT_6,
    DETECT_EVENT_7,
    DETECT_EVENT_8,
    DETECT_LIST_LENGTH,
};

void DetectTask_Init(void);
void DetectTask_Process(void);
void DetectTask_Hook(uint8_t event);
uint8_t DetectTask_IsOvertime(uint8_t event);

#endif /* __DETECT_TASK_H__ */
