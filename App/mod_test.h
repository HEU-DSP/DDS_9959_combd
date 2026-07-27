/**
 ******************************************************************************
 * @file    mod_test.h
 * @brief   Ozone-friendly modulation sweep test for AD9959 CH1 bring-up
 ******************************************************************************
 */

#ifndef __MOD_TEST_H__
#define __MOD_TEST_H__

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t enable;          /* 0=normal app, 1=run modulation test */
    uint32_t auto_advance;    /* 0=hold selected_index, 1=cycle modes */
    uint32_t selected_index;  /* 0..9: CW,FSK,ASK,GFSK,MSK,QPSK,AM,FM,BPSK,4FSK */
    uint32_t hold_ms;         /* dwell time per mode when auto_advance=1 */
    uint32_t force_apply;     /* write 1 in Ozone to re-apply selected mode */
} ModTestControl;

typedef struct {
    uint32_t active_index;
    uint32_t active_mode;
    uint32_t active_channel;
    uint32_t bits_per_symbol;
    uint32_t ftw0;
    uint32_t ftw1;
    uint32_t asf0;
    uint32_t asf1;
    uint32_t symbol_count;
    uint32_t apply_count;
    uint32_t feed_count;
    uint32_t last_tick;
    uint32_t switch_pending;
    uint32_t switch_count;
    uint32_t reinit_count;
} ModTestDebug;

extern volatile ModTestControl mod_test_ctrl;
extern volatile ModTestDebug mod_test_debug;

void ModTest_Init(uint32_t now_ms);
void ModTest_Task(uint32_t now_ms);
bool ModTest_HasPendingSwitch(void);
bool ModTest_ApplyPendingSwitch(void);

#endif /* __MOD_TEST_H__ */
