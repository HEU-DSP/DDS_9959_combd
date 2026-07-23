/**
  ******************************************************************************
  * @file    drv_74hc595_1.c
  * @brief   Driver for two daisy-chained 74HC595 shift registers
  *
  * Cascade: PE3 (DS) → Chip1 → Q7' → Chip2
  * Chip 1 [7:0]  = Status LEDs
  * Chip 2 [15:8] = DDS control
  *
  * Data is shifted MSB-first: the first bit (bit 15) ends up in Chip2 Q7,
  * the last bit (bit 0) ends up in Chip1 Q0 (NC).
  *
  * GPIOs are initialized by CubeMX MX_GPIO_Init() as OUTPUT_PP on GPIOE.
  *
  * Power-up sequence:
  *   1. Reset shift registers (all outputs low), OE disabled
  *   2. Enable DDS 1.8V digital supply (DDSPWREN1V8D)
  *   3. Enable DDS 1.8V analog supply (DDSPWREN1V8A)
  *   4. Wait 10ms for rails to stabilize
  *   5. Enable DDS 3.3V digital supply (DDSPWREN_3V3D)
  *   6. Set DDS REF_CLK mode to 3.3V (DDSCLKMODE33)
  *   7. Pulse DDS Master Reset (DDSMASTERRST low → delay → high)
  *   8. Enable outputs (OE# low)
  *
  * Reference: DOCS/引脚对照表.md
  ******************************************************************************
  */
#include "drv_74hc595_1.h"

uint16_t g_shiftreg_state;
volatile ShiftReg_State_t g_595;

/**
  * @brief  Initialize 74HC595 — GPIOs already configured by MX_GPIO_Init()
  */
void DRV_595_Init(void)
{
    /* GPIOs (PE2-PE6) are initialized by CubeMX MX_GPIO_Init() as OUTPUT_PP.
       Set initial levels: STCP=0, SHCP=0, DS=0, OE#=1 (disabled), MR#=1 (not reset) */
    HAL_GPIO_WritePin(OCR_STCP_PORT, OCR_STCP_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(OCR_SHCP_PORT, OCR_SHCP_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(OCR_DS_PORT,   OCR_DS_PIN,   GPIO_PIN_RESET);
    HAL_GPIO_WritePin(OCR_OE_PORT,   OCR_OE_PIN,   GPIO_PIN_SET);    /* disabled */
    HAL_GPIO_WritePin(OCR_MR_PORT,   OCR_MR_PIN,   GPIO_PIN_SET);    /* not reset */

    /* Reset shift registers to known state */
    DRV_595_Reset();

    g_shiftreg_state = 0x0000;
    g_595.raw = 0x0000;
}

/**
  * @brief  Shift out 16 bits MSB-first and latch
  * @param  data: [15:8]=Chip2 (DDS control), [7:0]=Chip1 (LEDs)
  * @note   Bit 15 shifted first → ends up in Chip2 Q7 (NC)
  *         Bit 0  shifted last  → ends up in Chip1 Q0 (NC)
  */
void DRV_595_Write(uint16_t data)
{
    uint16_t mask = 0x8000;

    /* STCP low during shift */
    HAL_GPIO_WritePin(OCR_STCP_PORT, OCR_STCP_PIN, GPIO_PIN_RESET);

    for (uint8_t i = 0; i < 16; i++)
    {
        /* Set DS */
        HAL_GPIO_WritePin(OCR_DS_PORT, OCR_DS_PIN,
                          (data & mask) ? GPIO_PIN_SET : GPIO_PIN_RESET);

        /* Pulse SHCP: high → NOPs → low */
        HAL_GPIO_WritePin(OCR_SHCP_PORT, OCR_SHCP_PIN, GPIO_PIN_SET);
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        HAL_GPIO_WritePin(OCR_SHCP_PORT, OCR_SHCP_PIN, GPIO_PIN_RESET);
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();

        mask >>= 1;
    }

    /* Latch: pulse STCP high */
    HAL_GPIO_WritePin(OCR_STCP_PORT, OCR_STCP_PIN, GPIO_PIN_SET);
    __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
    HAL_GPIO_WritePin(OCR_STCP_PORT, OCR_STCP_PIN, GPIO_PIN_RESET);

    g_shiftreg_state = data;
    g_595.raw = data;
}

/**
  * @brief  Set or clear a single bit in the shift register (auto-latch)
  */
void DRV_595_SetBit(uint16_t bit_mask, uint8_t value)
{
    uint16_t new_state;

    if (value)
        new_state = g_shiftreg_state | bit_mask;
    else
        new_state = g_shiftreg_state & ~bit_mask;

    if (new_state != g_shiftreg_state)
        DRV_595_Write(new_state);
}

/**
  * @brief  Enable/disable shift register outputs
  * @param  enable: 1 = outputs active (OE# low), 0 = Hi-Z (OE# high)
  */
void DRV_595_EnableOutput(uint8_t enable)
{
    HAL_GPIO_WritePin(OCR_OE_PORT, OCR_OE_PIN,
                      enable ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

/**
  * @brief  Pulse MR# low to reset both shift registers
  * @note   This clears the shift register, NOT the output register.
  *         Outputs hold their last latched value until next STCP pulse.
  */
void DRV_595_Reset(void)
{
    HAL_GPIO_WritePin(OCR_MR_PORT, OCR_MR_PIN, GPIO_PIN_RESET);
    for (volatile int i = 0; i < 100; i++) { __NOP(); }
    HAL_GPIO_WritePin(OCR_MR_PORT, OCR_MR_PIN, GPIO_PIN_SET);

    g_shiftreg_state = 0x0000;
    g_595.raw = 0x0000;
}

/**
  * @brief  Execute DDS power-up sequence
  *
  * Sequence (ref: DOCS/引脚对照表.md §第二片 595 并行输出):
  *   1. OE disabled, all outputs low
  *   2. Enable DDS 1.8V digital supply
  *   3. Enable DDS 1.8V analog supply
  *   4. Wait 10ms for rails
  *   5. Enable DDS 3.3V digital supply
  *   6. Set REF_CLK mode = 3.3V
  *   7. Master Reset pulse (low → 1ms → high)
  *   8. Enable outputs
  */
void DRV_595_PowerSeq_Init(void)
{
    uint16_t val;

    /* Step 1: All off, OE disabled, hold DDS in reset */
    DRV_595_EnableOutput(0);
    val = SHIFTREG_DDS_MASTERRST;               /* MASTER_RESET = HIGH (active high, DS p9) */
    DRV_595_Write(val);

    /* Step 2: Enable DDS 1.8V digital (MASTER_RESET stays HIGH) */
    val |= SHIFTREG_DDS_PWREN_1V8D;
    DRV_595_Write(val);

    /* Step 3: Enable DDS 1.8V analog */
    val |= SHIFTREG_DDS_PWREN_1V8A;
    DRV_595_Write(val);
    HAL_Delay(10);

    /* Step 4: Enable DDS 3.3V digital */
    val |= SHIFTREG_DDS_PWREN_3V3D;
    DRV_595_Write(val);

    /* Step 5: Set REF_CLK mode = 3.3V */
    val |= SHIFTREG_DDS_CLKMODE33;
    DRV_595_Write(val);

    /* DDS held in reset (MASTER_RESET=HIGH) until AD9959_Init releases it.
       This keeps DDS inactive while power rails stabilize. */

    /* Step 6: Enable outputs */
    DRV_595_EnableOutput(1);
    HAL_Delay(1);
}

/**
  * @brief  Safe power-down sequence (sleep mode)
  */
void DRV_595_PowerSeq_Sleep(void)
{
    uint16_t val;

    /* Power down DDS via DDSPDN, keep other states */
    val = g_shiftreg_state | SHIFTREG_DDS_PDN;
    DRV_595_Write(val);
}

/**
  * @brief  Sync debugger-modified g_595 state to 595 hardware.
  *         Call periodically in main loop.
  *         If g_595.raw was changed (via debugger watch window), writes to HW.
  * @note   Rate-limited to ~100ms between actual writes to avoid bus thrashing.
  */
void DRV_595_Refresh(void)
{
    if (g_595.raw == g_shiftreg_state)
        return;

    static uint32_t last_tick = 0;
    uint32_t now = HAL_GetTick();
    if (now - last_tick >= 100)
    {
        last_tick = now;
        DRV_595_Write(g_595.raw);
    }
}
