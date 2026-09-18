/**
 ******************************************************************************
 * @file    aps6404.c
 * @brief   APS6404L QSPI PSRAM driver — see aps6404.h
 ******************************************************************************
 */

#include "aps6404.h"
#include "main.h"
#include <string.h>

/* CubeMX-generated handle lives at file scope in main.c (same convention
 * as hspi1 — it is not declared in any header). */
extern OSPI_HandleTypeDef hospi1;

volatile APS6404_Diag aps6404_diag = {0};

/* ================================================================
 * Command plumbing (all through HAL_OSPI_Command)
 * ================================================================ */

/* Every field is set explicitly: OSPI_ConfigCmd overwrites CCR/WCCR with
 * the whole struct (hal_ospi.c:2978), so a `= {0}` template is mandatory
 * or stale garbage lands in the registers. */
static OSPI_RegularCmdTypeDef aps6404_cmd_base(void)
{
    OSPI_RegularCmdTypeDef c = {0};
    c.FlashId             = HAL_OSPI_FLASH_ID_1;
    c.InstructionSize     = HAL_OSPI_INSTRUCTION_8_BITS;
    c.InstructionDtrMode  = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
    c.AddressSize         = HAL_OSPI_ADDRESS_24_BITS;
    c.AddressDtrMode      = HAL_OSPI_ADDRESS_DTR_DISABLE;
    c.AlternateBytesMode  = HAL_OSPI_ALTERNATE_BYTES_NONE;
    c.AlternateBytesDtrMode = HAL_OSPI_ALTERNATE_BYTES_DTR_DISABLE;
    c.DataDtrMode         = HAL_OSPI_DATA_DTR_DISABLE;
    c.SIOOMode            = HAL_OSPI_SIOO_INST_EVERY_CMD;
    return c;
}

static HAL_StatusTypeDef aps6404_send_cmd(const OSPI_RegularCmdTypeDef *c)
{
    return HAL_OSPI_Command(&hospi1, (OSPI_RegularCmdTypeDef *)c,
                            HAL_OSPI_TIMEOUT_DEFAULT_VALUE);
}

/* Read config: 0xEB Fast Read Quad I/O, 4-4-4, 24-bit address, 6 dummy.
 * DQS must stay DISABLED — the APS6404 has no DQS pin. */
static HAL_StatusTypeDef aps6404_config_read(void)
{
    OSPI_RegularCmdTypeDef c = aps6404_cmd_base();
    c.OperationType   = HAL_OSPI_OPTYPE_READ_CFG;
    c.Instruction     = 0xEBU;
    c.InstructionMode = HAL_OSPI_INSTRUCTION_4_LINES;
    c.AddressMode     = HAL_OSPI_ADDRESS_4_LINES;
    c.DataMode        = HAL_OSPI_DATA_4_LINES;
    c.DummyCycles     = 6U;
    c.DQSMode         = HAL_OSPI_DQS_DISABLE;
    return aps6404_send_cmd(&c);
}

/* Write config: 0x38 Quad Write, 4-4-4, 0 dummy.
 * DQSE is set on the write path even though the chip has no DQS pin —
 * H723 errata ES0491 §2.7.6 (memory-mapped write error response when DQS
 * output is disabled) lists this as the partial workaround. */
static HAL_StatusTypeDef aps6404_config_write(void)
{
    OSPI_RegularCmdTypeDef c = aps6404_cmd_base();
    c.OperationType   = HAL_OSPI_OPTYPE_WRITE_CFG;
    c.Instruction     = 0x38U;
    c.InstructionMode = HAL_OSPI_INSTRUCTION_4_LINES;
    c.AddressMode     = HAL_OSPI_ADDRESS_4_LINES;
    c.DataMode        = HAL_OSPI_DATA_4_LINES;
    c.DummyCycles     = 0U;
    c.DQSMode         = HAL_OSPI_DQS_ENABLE;
    return aps6404_send_cmd(&c);
}

static HAL_StatusTypeDef aps6404_enter_mapped(void)
{
    OSPI_MemoryMappedTypeDef mm = {0};
    mm.TimeOutActivation = HAL_OSPI_TIMEOUT_COUNTER_DISABLE; /* tCEM via Init.Refresh */
    return HAL_OSPI_MemoryMapped(&hospi1, &mm);
}

/* ================================================================
 * Public API
 * ================================================================ */

void APS6404_Init(void)
{
    aps6404_diag = (APS6404_Diag){0};
    aps6404_diag.uses_indirect_write = 1U;

    /* 1. Power-up settle.  The PSRAM rail is always-on, so a generous
     *    HAL_Delay(1) covers the >= 150 us tPU requirement with room. */
    HAL_Delay(1);
    aps6404_diag.init_stage = 1U;

    /* 2. Software reset (0x66 Reset-Enable, 0x99 Reset).  Both opcodes are
     *    nibble-symmetric, so a single-line transfer decodes correctly in
     *    SPI AND QPI mode — after a warm MCU reset the chip may still hold
     *    its QPI latch, and a plain 0x35 would be misinterpreted. */
    {
        OSPI_RegularCmdTypeDef c = aps6404_cmd_base();
        c.OperationType   = HAL_OSPI_OPTYPE_COMMON_CFG;
        c.InstructionMode = HAL_OSPI_INSTRUCTION_1_LINE;
        c.AddressMode     = HAL_OSPI_ADDRESS_NONE;
        c.DataMode        = HAL_OSPI_DATA_NONE;
        c.DummyCycles     = 0U;
        c.DQSMode         = HAL_OSPI_DQS_DISABLE;

        c.Instruction = 0x66U;
        if (aps6404_send_cmd(&c) != HAL_OK) return;
        c.Instruction = 0x99U;
        if (aps6404_send_cmd(&c) != HAL_OK) return;
    }
    HAL_Delay(1);   /* tRST not confirmed in the datasheet — be generous */
    aps6404_diag.init_stage = 2U;

    /* 3. Enter QPI mode.  DataMode=NONE leaves the HAL state READY, so the
     *    following config commands chain without re-entry. */
    {
        OSPI_RegularCmdTypeDef c = aps6404_cmd_base();
        c.OperationType   = HAL_OSPI_OPTYPE_COMMON_CFG;
        c.Instruction     = 0x35U;
        c.InstructionMode = HAL_OSPI_INSTRUCTION_1_LINE;
        c.AddressMode     = HAL_OSPI_ADDRESS_NONE;
        c.DataMode        = HAL_OSPI_DATA_NONE;
        c.DummyCycles     = 0U;
        c.DQSMode         = HAL_OSPI_DQS_DISABLE;
        if (aps6404_send_cmd(&c) != HAL_OK) return;
    }
    aps6404_diag.init_stage = 3U;

    /* 4-6. Read cfg → write cfg → memory-mapped (HAL state machine order). */
    if (aps6404_config_read() != HAL_OK)  return;
    aps6404_diag.init_stage = 4U;
    if (aps6404_config_write() != HAL_OK) return;
    aps6404_diag.init_stage = 5U;
    if (aps6404_enter_mapped() != HAL_OK) return;
    aps6404_diag.init_stage = 6U;

    /* Probe read through the mapped window before declaring ready. */
    {
        uint8_t probe[4];
        APS6404_Read(0U, probe, sizeof(probe));
        aps6404_diag.ready = 1U;
    }
}

uint8_t APS6404_IsReady(void)
{
    return aps6404_diag.ready;
}

void APS6404_SetIndirectWrite(uint8_t on)
{
    aps6404_diag.uses_indirect_write = (on != 0U) ? 1U : 0U;
}

void APS6404_Read(uint32_t off, uint8_t *buf, uint32_t len)
{
    if ((buf == NULL) || (len == 0U)
        || ((uint64_t)off + (uint64_t)len > APS6404_SIZE_BYTES)) {
        aps6404_diag.bounds_err++;
        return;
    }
    memcpy(buf, (const void *)(uintptr_t)(APS6404_BASE + off), len);
}

void APS6404_Write(uint32_t off, const uint8_t *buf, uint32_t len)
{
    if ((buf == NULL) || (len == 0U)
        || ((uint64_t)off + (uint64_t)len > APS6404_SIZE_BYTES)) {
        aps6404_diag.bounds_err++;
        return;
    }

    if (aps6404_diag.uses_indirect_write == 0U) {
        /* Fast path: direct write through the mapped window.  Carries the
         * ES0491 §2.7.6 risk — only enabled after the mapped-write self-test
         * passes on this silicon. */
        memcpy((void *)(uintptr_t)(APS6404_BASE + off), buf, len);
        return;
    }

    /* Indirect path: leave mapped mode, write per 1 KB page (linear bursts
     * tolerate at most one page crossing, and tCEM is unverified), then
     * re-enter mapped mode.  READY cannot jump straight to MemoryMapped,
     * hence the three-call re-entry. */
    if (HAL_OSPI_Abort(&hospi1) != HAL_OK) {
        aps6404_diag.fail_addr = off;
        return;
    }

    while (len > 0U) {
        uint32_t chunk = APS6404_PAGE_SIZE - (off % APS6404_PAGE_SIZE);
        if (chunk > len) {
            chunk = len;
        }

        OSPI_RegularCmdTypeDef c = aps6404_cmd_base();
        c.OperationType   = HAL_OSPI_OPTYPE_COMMON_CFG;
        c.Instruction     = 0x38U;
        c.InstructionMode = HAL_OSPI_INSTRUCTION_4_LINES;
        c.Address         = off;
        c.AddressMode     = HAL_OSPI_ADDRESS_4_LINES;
        c.DataMode        = HAL_OSPI_DATA_4_LINES;
        c.NbData          = chunk;      /* writes DLR — Transmit takes its length from it */
        c.DummyCycles     = 0U;
        c.DQSMode         = HAL_OSPI_DQS_DISABLE;  /* workaround targets WCCR/mapped only */

        if (aps6404_send_cmd(&c) != HAL_OK) {
            aps6404_diag.fail_addr = off;
            break;
        }
        if (HAL_OSPI_Transmit(&hospi1, (uint8_t *)buf,
                              HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
            aps6404_diag.fail_addr = off;
            break;
        }

        off  += chunk;
        buf  += chunk;
        len  -= chunk;
    }

    (void)aps6404_config_read();
    (void)aps6404_config_write();
    (void)aps6404_enter_mapped();
}

/* ================================================================
 * Self-test (boot-time diagnostics; results into aps6404_diag, never fatal)
 * ================================================================ */

static uint8_t aps6404_pattern(uint32_t off, uint32_t i)
{
    /* Address-mixed walking pattern — catches stuck bits, shorts and
     * address-line swaps. */
    return (uint8_t)(0xA5U ^ (uint8_t)i ^ (uint8_t)(off >> ((i & 7U) + 1U)));
}

static uint8_t aps6404_verify(const uint8_t *got, uint32_t off, uint32_t len)
{
    for (uint32_t i = 0U; i < len; i++) {
        if (got[i] != aps6404_pattern(off, i)) {
            aps6404_diag.fail_addr = off + i;
            return 0U;
        }
    }
    return 1U;
}

void APS6404_SelfTest(void)
{
    static uint8_t buf[256];                      /* CPU-side scratch   */
    static uint8_t dma_buf[256]
        __attribute__((section(".dma_buffer"), aligned(32))); /* DMA-reachable (RAM_D2) */

    aps6404_diag.cpu_test = 0U;
    aps6404_diag.unaligned_test = 0U;
    aps6404_diag.pagecross_test = 0U;
    aps6404_diag.mapped_write_test = 0U;
    aps6404_diag.mapped_write_mismatch = 0U;
    aps6404_diag.mapped_fail_count = 0U;
    aps6404_diag.dma_test = 0U;
    aps6404_diag.dma_error_code = 0U;
    aps6404_diag.fail_addr = 0U;

    /* A. CPU pattern: three probes + a full-aperture sweep.  The write at
     *    0x7FFFF0 (and the sweep past 256 B) fails if DeviceSize is wrong —
     *    with DeviceSize=8 only the first 256 B of the aperture exists.
     *    The last probe is 16 B because 0x7FFFF0 is only 16 B from the end
     *    of the 8 MB aperture. */
    {
        uint8_t ok = 1U;
        const struct { uint32_t off; uint32_t len; } probes[3] = {
            {0U, 32U}, {0x1000U, 32U}, {0x7FFFF0U, 16U},
        };

        for (uint32_t p = 0U; p < 3U; p++) {
            uint32_t off = probes[p].off;
            uint32_t len = probes[p].len;
            for (uint32_t i = 0U; i < len; i++) {
                buf[i] = aps6404_pattern(off, i);
            }
            APS6404_Write(off, buf, len);
            APS6404_Read(off, buf, len);
            if (!aps6404_verify(buf, off, len)) {
                ok = 0U;
            }
        }
        for (uint32_t off = 0U; (off < APS6404_SIZE_BYTES) && (ok != 0U);
             off += 65536U) {
            for (uint32_t i = 0U; i < 256U; i++) {
                buf[i] = aps6404_pattern(off, i);
            }
            APS6404_Write(off, buf, 256U);
            APS6404_Read(off, buf, 256U);
            if (!aps6404_verify(buf, off, 256U)) {
                ok = 0U;
            }
        }
        aps6404_diag.cpu_test = ok ? 1U : 2U;
    }

    /* B. Unaligned byte access — faults if the MPU entry ever degrades to
     *    Device/Strongly-ordered. */
    {
        uint8_t ok = 1U;
        for (uint32_t i = 0U; i < 16U; i++) {
            buf[i] = aps6404_pattern(0x101U, i);
        }
        APS6404_Write(0x101U, buf, 16U);
        APS6404_Read(0x101U, buf, 16U);
        if (!aps6404_verify(buf, 0x101U, 16U)) {
            ok = 0U;
        }
        aps6404_diag.unaligned_test = ok ? 1U : 2U;
    }

    /* C. Page-cross write (0x3F0 + 256 B spans the 1 KB boundary) via the
     *    indirect path — exercises multi-chunk writes and tCEM. */
    {
        uint8_t ok = 1U;
        for (uint32_t i = 0U; i < 256U; i++) {
            buf[i] = aps6404_pattern(0x3F0U, i);
        }
        APS6404_Write(0x3F0U, buf, 256U);
        APS6404_Read(0x3F0U, buf, 256U);
        if (!aps6404_verify(buf, 0x3F0U, 256U)) {
            ok = 0U;
        }
        aps6404_diag.pagecross_test = ok ? 1U : 2U;
    }

    /* D. Mapped-write A/B: write 4 KB via the mapped window and count
     *    mismatches; then repeat via the indirect path (known-good) to
     *    separate "mapped writes broken" from "pattern/read broken". */
    {
        uint32_t mismatch = 0U;

        APS6404_SetIndirectWrite(0U);
        for (uint32_t off = 0U; off < 4096U; off += 256U) {
            for (uint32_t i = 0U; i < 256U; i++) {
                buf[i] = aps6404_pattern(off, i);
            }
            APS6404_Write(off, buf, 256U);
            APS6404_Read(off, buf, 256U);
            for (uint32_t i = 0U; i < 256U; i++) {
                if (buf[i] != aps6404_pattern(off, i)) {
                    mismatch++;
                    if (aps6404_diag.mapped_fail_count < 4U) {
                        aps6404_diag.mapped_fail_off[aps6404_diag.mapped_fail_count] = off + i;
                        aps6404_diag.mapped_fail_count++;
                    }
                }
            }
        }
        aps6404_diag.mapped_write_mismatch = mismatch;

        APS6404_SetIndirectWrite(1U);   /* known-good path for the A/B */
        for (uint32_t off = 0U; off < 4096U; off += 256U) {
            for (uint32_t i = 0U; i < 256U; i++) {
                buf[i] = aps6404_pattern(off, i);
            }
            APS6404_Write(off, buf, 256U);
            APS6404_Read(off, buf, 256U);
            if (!aps6404_verify(buf, off, 256U)) {
                break;
            }
        }
        /* Indirect leg must be clean for the test to count as a pass. */
        aps6404_diag.mapped_write_test =
            (mismatch == 0U) && (aps6404_diag.fail_addr == 0U) ? 1U : 2U;
        /* Stay on the safe path until a soak decides otherwise. */
    }

    /* E. DMA probe (diagnostic only — the architecture does not need DMA to
     *    reach PSRAM).  DMA1_Stream2 is free (0=USART1_RX, 1=SPI1_TX,
     *    3=TIM4_UP); M2M needs no request line and no NVIC. */
    {
        static DMA_HandleTypeDef hdma_m2m;
        hdma_m2m.Instance                 = DMA1_Stream2;
        hdma_m2m.Init.Request             = DMA_REQUEST_MEM2MEM;
        hdma_m2m.Init.Direction           = DMA_MEMORY_TO_MEMORY;
        hdma_m2m.Init.PeriphInc           = DMA_PINC_ENABLE;
        hdma_m2m.Init.MemInc              = DMA_MINC_ENABLE;
        hdma_m2m.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_m2m.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
        hdma_m2m.Init.Mode                = DMA_NORMAL;
        hdma_m2m.Init.Priority            = DMA_PRIORITY_LOW;
        hdma_m2m.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;

        if (HAL_DMA_Init(&hdma_m2m) != HAL_OK) {
            aps6404_diag.dma_test = 3U;
        } else {
            uint8_t ok = 1U;

            /* SRAM → PSRAM, then CPU read-back compare. */
            for (uint32_t i = 0U; i < 256U; i++) {
                dma_buf[i] = aps6404_pattern(0x2000U, i);
            }
            if (HAL_DMA_Start(&hdma_m2m, (uint32_t)(uintptr_t)dma_buf,
                              APS6404_BASE + 0x2000U, 256U) != HAL_OK) {
                ok = 0U;
            } else if (HAL_DMA_PollForTransfer(&hdma_m2m, HAL_DMA_FULL_TRANSFER,
                                               100U) != HAL_OK) {
                ok = 0U;
            } else {
                APS6404_Read(0x2000U, buf, 256U);
                if (!aps6404_verify(buf, 0x2000U, 256U)) {
                    ok = 0U;
                }
            }

            /* PSRAM → SRAM: CPU seeds the pattern (indirect), DMA copies
             * back into dma_buf, CPU compares. */
            for (uint32_t i = 0U; i < 256U; i++) {
                buf[i] = aps6404_pattern(0x3000U, i);
            }
            APS6404_Write(0x3000U, buf, 256U);
            if (ok != 0U) {
                if (HAL_DMA_Start(&hdma_m2m, APS6404_BASE + 0x3000U,
                                  (uint32_t)(uintptr_t)dma_buf, 256U) != HAL_OK) {
                    ok = 0U;
                } else if (HAL_DMA_PollForTransfer(&hdma_m2m,
                                                   HAL_DMA_FULL_TRANSFER,
                                                   100U) != HAL_OK) {
                    ok = 0U;
                } else if (!aps6404_verify(dma_buf, 0x3000U, 256U)) {
                    ok = 0U;
                }
            }

            aps6404_diag.dma_error_code = hdma_m2m.ErrorCode;
            if (ok != 0U) {
                aps6404_diag.dma_test = 1U;
            } else {
                aps6404_diag.dma_test =
                    (hdma_m2m.ErrorCode == HAL_DMA_ERROR_TE) ? 2U : 3U;
            }
        }
    }
}
