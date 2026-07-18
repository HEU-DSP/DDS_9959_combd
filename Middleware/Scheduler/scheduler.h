/**
 ******************************************************************************
 * @file    scheduler.h
 * @brief   Transmission Scheduler — Skeleton (Phase 3)
 ******************************************************************************
 */

#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include "dds_command.h"

void Scheduler_Init(void);
int  Scheduler_Enqueue(const DDS_Command *cmds, int num);
int  Scheduler_Dequeue(DDS_Command *cmds, int max_cmds);

#endif /* __SCHEDULER_H__ */
