/**
 ******************************************************************************
 * @file    aps6404.h
 * @brief   APS6404 QSPI PSRAM Driver — Skeleton (Phase 3)
 ******************************************************************************
 */

#ifndef __APS6404_H__
#define __APS6404_H__

#include <stdint.h>

void APS6404_Init(void);
void APS6404_Read(uint32_t addr, uint8_t *buf, uint32_t len);
void APS6404_Write(uint32_t addr, const uint8_t *buf, uint32_t len);

#endif /* __APS6404_H__ */
