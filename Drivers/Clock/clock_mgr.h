/**
 ******************************************************************************
 * @file    clock_mgr.h
 * @brief   Clock Distribution Manager — Skeleton (Phase 3)
 ******************************************************************************
 */

#ifndef __CLOCK_MGR_H__
#define __CLOCK_MGR_H__

#include <stdint.h>

void ClockMgr_Init(void);
void ClockMgr_SetREFCLK(uint32_t freq_hz);
uint32_t ClockMgr_GetSYSCLK(void);

#endif /* __CLOCK_MGR_H__ */
