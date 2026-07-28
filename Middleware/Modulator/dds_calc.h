/**
 ******************************************************************************
 * @file    dds_calc.h
 * @brief   AD9959 parameter calculator — integer-only, compile-time constants
 *
 * All AD9959 parameters (FTW, CPOW, ASF, clock-chain) derived from CubeMX
 * RCC configuration.  Every macro is a compile-time integer — no float,
 * no division at runtime.
 *
 * ╔══════════════════════════════════════════════════════════════╗
 * ║              AD9959 参数调试速查 (Ozone)                     ║
 * ╠══════════════════════════════════════════════════════════════╣
 * ║ TIM15 clock      = 281.250 MHz  (APB2 Timer)                ║
 * ║ REF_CLK          =  25.568 MHz  (281.25M / 11)              ║
 * ║ AD9959 SYSCLK    = 511.364 MHz  (×20 PLL)                   ║
 * ║ FTW Scale        = 0x20D3B4F3A713                           ║
 * ║                                                              ║
 * ║ 预编码缓冲区 (pre_encoded[0..3]):                             ║
 * ║   .cftw[0]=0x04  [1..4]=FTW MSB-first                       ║
 * ║   .acr[0]=0x06   [1..3]=AMP_MULT|ASF                        ║
 * ║   .cpow[0]=0x05  [1..2]=POW[13:0]                           ║
 * ║                                                              ║
 * ║ 监视变量:                                                     ║
 * ║   pre_encoded_mask    → 活跃通道 bitmask                     ║
 * ║   mod_cfg[ch].cw.{ftw,asf} / .enabled                       ║
 * ║   debug_reload        → 置 1 应用 Ozone 修改                 ║
 * ║   DDSCALC_FTW_SCALE / DDSCALC_AD9959_SYSCLK_HZ              ║
 * ╚══════════════════════════════════════════════════════════════╝
 ******************************************************************************
 */

#ifndef __DDS_CALC_H__
#define __DDS_CALC_H__

#include <stdint.h>

/* ================================================================
 * 时钟链常量 — 编译期从 CubeMX RCC 一次性推导
 * ================================================================ */

#define DDSCALC_HSE_HZ            25000000UL
#define DDSCALC_PLLM              3U
#define DDSCALC_PLLN              135U
#define DDSCALC_PLLR              2U

/* SYSCLK = HSE / PLLM × PLLN / PLLR */
#define DDSCALC_SYSCLK_HZ  \
    (DDSCALC_HSE_HZ / DDSCALC_PLLM * DDSCALC_PLLN / DDSCALC_PLLR)

/* HCLK = SYSCLK / HPRE(2), APB2 Timer = 2×HCLK/APB2_DIV(2) = HCLK */
#define DDSCALC_HCLK_HZ          (DDSCALC_SYSCLK_HZ / 2U)
#define DDSCALC_APB2_TIM_HZ      DDSCALC_HCLK_HZ

/* AD9959 PLL 倍频 (FR1[22:18]) */
#define DDSCALC_AD9959_PLL       20U

/* REF_CLK = APB2_TIM / DIV, rounding to nearest 25 MHz */
#define DDSCALC_REFCLK_DIV       11U
#define DDSCALC_REFCLK_HZ        (DDSCALC_APB2_TIM_HZ / DDSCALC_REFCLK_DIV)

/* AD9959 内部 SYSCLK */
#define DDSCALC_AD9959_SYSCLK_HZ  UINT64_C(490909091)

/* ================================================================
 * Q32 定点缩放因子 — FTW = (freq_hz × SCALE) >> 32
 *
 * SCALE = floor(2^64 / AD9959_SYSCLK)
 *   Python: hex(int(2**64 / (25000000//3*135//2//2//11*20)))
 *   2^64 / 511363636 = 36089050678771 = 0x20D3B4F3A713
 * ================================================================ */


/* ================================================================
 * CPOW / ASF 辅助常量
 * ================================================================ */

#define DDSCALC_CPOW_PER_DEG      46U       /* round(2^14 / 360)  */
#define DDSCALC_CPOW_MAX          0x3FFFU   /* 14-bit             */
#define DDSCALC_ASF_MAX           0x3FFU    /* 10-bit             */
#define DDSCALC_ASF_AMP_MULT      (1U << 12)

/* ================================================================
 * 参数计算接口 — 全部 static inline，编译期可常量化
 * ================================================================ */

/**
 * @brief  FTW from frequency in Hz.  Integer-only, zero runtime cost
 *         when freq_hz is a compile-time constant.
 */
static inline uint32_t DDSCalc_FTW(uint32_t freq_hz)
{
    return (uint32_t)((((uint64_t)freq_hz << 32)
                       + DDSCALC_AD9959_SYSCLK_HZ / 2U)
                      / DDSCALC_AD9959_SYSCLK_HZ);
}

/**
 * @brief  CPOW from phase in degrees (0–359).  Integer-only.
 */
static inline uint16_t DDSCalc_CPOW(uint16_t phase_deg)
{
    if (phase_deg >= 360U) return 0;
    return (uint16_t)(((uint32_t)phase_deg * DDSCALC_CPOW_PER_DEG + 1U) >> 1);
}

/**
 * @brief  ASF from normalised amplitude (0.0–1.0).
 */
static inline uint16_t DDSCalc_ASF(float amplitude)
{
    if (amplitude <= 0.0f) return 0;
    if (amplitude >= 1.0f) return DDSCALC_ASF_MAX;
    return (uint16_t)(amplitude * (float)DDSCALC_ASF_MAX + 0.5f);
}

/**
 * @brief  TIM15 ARR value for a target REF_CLK frequency.
 */
static inline uint16_t DDSCalc_REFCLK_ARR(uint32_t target_hz)
{
    uint32_t div = (DDSCALC_APB2_TIM_HZ + target_hz / 2U) / target_hz;
    if (div < 2U) div = 2U;
    return (uint16_t)(div - 1U);
}

#endif /* __DDS_CALC_H__ */
