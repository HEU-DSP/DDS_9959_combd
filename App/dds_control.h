/**
 ******************************************************************************
 * @file    dds_control.h
 * @brief   Staging config (pending_cfg) + APPLY safe-switch controller.
 *
 * The host sends SET_xxx commands that write into `pending_cfg`.
 * On CMD_APPLY, DDSControl_Task atomically applies pending_cfg to
 * the live DDS chain:
 *   1. Pause TIM8, wait SPI1 idle
 *   2. Copy pending_cfg to mod_cfg[]
 *   3. Switch PC6 to GPIO, write CSR+CFR static regs, GPIO IO_UPDATE
 *   4. Rebuild pre_encoded[]
 *   5. Restore PC6 to TIM8_CH1 AF, resume TIM8
 *
 * This module never touches REF_CLK, 595, PLL/FR1/FR2, MASTER_RESET
 * or any power/reset sequence.
 ******************************************************************************
 */

#ifndef __DDS_CONTROL_H__
#define __DDS_CONTROL_H__

#include <stdint.h>
#include <stdbool.h>

/* DDS safe-switch result codes */
#define DDS_APPLY_OK         0U
#define DDS_APPLY_ERR_BUSY   1U
#define DDS_APPLY_ERR_SPI    2U
#define DDS_APPLY_ERR_TIM    3U

/**
 * @brief  Signal that pending_cfg has been populated and should be
 *         applied at the next safe opportunity.
 *         Called from host protocol handler (main-loop context).
 */
void DDSControl_RequestApply(void);

/**
 * @brief  Signal stop-output (disable all channels, keep DDS alive).
 */
void DDSControl_RequestStop(void);

/**
 * @brief  Poll in main loop. Executes the APPLY or STOP safe-switch
 *         when requested. Returns immediately if no request pending.
 * @return DDS_APPLY_OK, or error code.
 */
uint8_t DDSControl_Task(void);

/**
 * @brief  Check whether an APPLY or STOP is in progress.
 */
bool DDSControl_IsBusy(void);

#endif /* __DDS_CONTROL_H__ */
