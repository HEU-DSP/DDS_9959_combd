/**
 ******************************************************************************
 * @file    aps6404.h
 * @brief   APS6404L QSPI PSRAM driver — OCTOSPI1 memory-mapped
 *
 * 8 MB mapped at 0x90000000 (MPU region 7: Normal, non-cacheable, XN).
 * Init: software reset (0x66/0x99) → QPI entry (0x35) → read config
 * (0xEB, 4-4-4, 6 dummy) → write config (0x38, 4-4-4, 0 dummy, DQSE per
 * H723 errata ES0491 §2.7.6) → HAL_OSPI_MemoryMapped.
 *
 * Writes are dual-path (aps6404_diag.uses_indirect_write):
 *   0 = mapped memcpy (fast; H723 memory-mapped writes carry an errata —
 *       enable only after the mapped-write self-test passes on this silicon)
 *   1 = indirect 0x38 per 1 KB page (default, known-good)
 *
 * No App/Middleware buffers are mapped to this region yet — the driver
 * only establishes CPU/DMA-accessible linear space as the foundation.
 ******************************************************************************
 */

#ifndef __APS6404_H__
#define __APS6404_H__

#include <stdint.h>

#define APS6404_BASE         0x90000000UL
#define APS6404_SIZE_BYTES   (8UL * 1024UL * 1024UL)
#define APS6404_PAGE_SIZE    1024U

/* Ozone-visible bring-up diagnostics.  Every field is written by
 * APS6404_Init()/APS6404_SelfTest() only — never fatal, never from ISR. */
typedef struct {
    uint8_t  ready;               /* 1 = QPI entered + mapped + probe read OK     */
    uint8_t  uses_indirect_write; /* 0 = mapped memcpy, 1 = indirect 0x38         */
    uint8_t  init_stage;          /* last reached init step (1..6)                */
    uint8_t  cpu_test;            /* 0 not run, 1 pass, 2 fail                    */
    uint8_t  unaligned_test;      /* 0 not run, 1 pass, 2 fail                    */
    uint8_t  pagecross_test;      /* 0 not run, 1 pass, 2 fail                    */
    uint8_t  mapped_write_test;   /* 0 not run, 1 pass, 2 fail                    */
    uint8_t  dma_test;            /* 0 not run, 1 pass, 2 = TE bus error, 3 other */
    uint32_t mapped_write_mismatch; /* mismatched bytes on the mapped-write test  */
    uint32_t dma_error_code;        /* hdma.ErrorCode of the DMA probe            */
    uint32_t fail_addr;             /* first failing PSRAM offset                 */
    uint32_t bounds_err;            /* out-of-range Read/Write calls              */
    uint8_t  mapped_fail_count;     /* how many of mapped_fail_off[] are valid    */
    uint32_t mapped_fail_off[4];    /* first few mismatching offsets (geometry)   */
} APS6404_Diag;

extern volatile APS6404_Diag aps6404_diag;

/**
 * @brief  Reset + QPI + command configs + memory-mapped mode.  Never fatal.
 */
void APS6404_Init(void);

/**
 * @brief  Boot-time bring-up diagnostics (CPU pattern, unaligned, page
 *         cross, mapped-write A/B, DMA probe).  Results into aps6404_diag.
 */
void APS6404_SelfTest(void);

/** @return 1 when the mapped region has been probe-verified. */
uint8_t APS6404_IsReady(void);

/**
 * @brief  Select the write path at runtime.
 * @param  on : 1 = indirect 0x38 (safe default), 0 = mapped memcpy (fast)
 */
void APS6404_SetIndirectWrite(uint8_t on);

/** @brief  Read from PSRAM via the mapped region (memcpy, bounds-checked). */
void APS6404_Read(uint32_t off, uint8_t *buf, uint32_t len);

/** @brief  Write to PSRAM — path per aps6404_diag.uses_indirect_write. */
void APS6404_Write(uint32_t off, const uint8_t *buf, uint32_t len);

#endif /* __APS6404_H__ */
