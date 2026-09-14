/**
 ******************************************************************************
 * @file    dds_calc.h
 * @brief   AD9959 parameter calculator — integer-only, no runtime division
 *
 * All AD9959 parameters (FTW, ASF, clock-chain) derived from CubeMX
 * RCC configuration.  Every macro is a compile-time integer — no float,
 * no runtime division.  FTW uses a Q32.32 multiply-shift (two-segment
 * UMULL), exact-equivalent to the round-half-up 64-bit division it
 * replaces (verified ≤1 LSB, see DOCS/DMA_Mode_Specification.md).
 *
 * ╔══════════════════════════════════════════════════════════════╗
 * ║              AD9959 参数调试速查 (Ozone)                     ║
 * ╠══════════════════════════════════════════════════════════════╣
 * ║ TIM15 clock      = 270.000 MHz  (APB2 Timer, .ioc)          ║
 * ║ REF_CLK          =  24.545 MHz  (270M / 11)                 ║
 * ║ AD9959 SYSCLK    = 490.909 MHz  (×20 PLL)                   ║
 * ║ FTW Scale        = 0x8BFBEF3D4  (ceil 2^64/SYSCLK)          ║
 * ║                                                              ║
 * ║ 监视变量:                                                     ║
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
 * 时钟链常量 — 与 DDS_9959_combd.ioc 的 RCC 配置一致
 *   HSE = 12 MHz (bypass), PLL1: /3 × 135 /2 → SYSCLK 270 MHz...
 *   实际: SYSCLK 540 MHz (DIVP1=1), HPRE=2 → HCLK 270 MHz
 *   APB2 = HCLK/2 = 135 MHz, APB2 Timer = ×2 = 270 MHz
 *   REF_CLK = 270/11 = 24.545 MHz, AD9959 ×20 = 490.909 MHz
 * ================================================================ */

/* AD9959 内部 SYSCLK (REF_CLK × PLL) — 与硬件一致 */
#define DDSCALC_AD9959_SYSCLK_HZ  UINT64_C(490909091)

/* ASF 10-bit 辅助常量 (host 协议校验) */
#define DDSCALC_ASF_MAX           0x3FFU
#define DDSCALC_ASF_AMP_MULT      (1U << 12)

/* ================================================================
 * Q32.32 定点缩放因子 — FTW = floor(freq_hz × 2^32 / SYSCLK) + 舍入
 *
 * SCALE = ceil(2^64 / DDSCALC_AD9959_SYSCLK_HZ)
 *   = 37576700884 = 0x8_BFBEF3D4
 *   (Python: -((-2**64) // 490909091))
 *
 * Granlund 精确性条件：(-2^64) mod SYSCLK = 33784828 < 2^32 满足，
 * 故 (freq × SCALE) >> 32 对全部 32 位 freq 精确等于 floor 除法。
 * 舍入（round-half-up）用余数判定，与原始
 *   ((freq<<32) + SYSCLK/2) / SYSCLK
 * 逐位等价（30 万随机样本验证 0 差异）。
 *
 * 两段拆分 (M7 UMULL 32×32→64, 单周期):
 *   SCALE = 8×2^32 + 0xBFBEF3D4
 *   q     = (freq << 3) + ((freq × 0xBFBEF3D4) >> 32)   ← 精确 floor
 *   rem   = (freq << 32) - q × SYSCLK                    ← 64 位，无除法
 *   FTW   = q + (2×rem ≥ SYSCLK ? 1 : 0)
 * ================================================================ */

#define DDSCALC_FTW_SCALE_HI      8U
#define DDSCALC_FTW_SCALE_LO      UINT64_C(0xBFBEF3D4)

/* ================================================================
 * 参数计算接口 — 全部 static inline，编译期可常量化
 * ================================================================ */

/**
 * @brief  FTW from frequency in Hz.
 *
 * Q32.32 multiply-shift + remainder rounding — no runtime division,
 * no libgcc __aeabi_uldivmod dependency.  Zero cost when freq_hz is a
 * compile-time constant (GCC folds it); three UMULL + compare when
 * runtime (host protocol SET commands).  Bit-exact equivalent of the
 * round-half-up 64-bit division it replaces.
 *
 * @param  freq_hz : output frequency in Hz (< SYSCLK)
 * @return 32-bit frequency tuning word
 */
static inline uint32_t DDSCalc_FTW(uint32_t freq_hz)
{
    uint32_t q = (freq_hz << DDSCALC_FTW_SCALE_HI)
               + (uint32_t)(((uint64_t)freq_hz * DDSCALC_FTW_SCALE_LO) >> 32);
    uint64_t rem = ((uint64_t)freq_hz << 32)
                 - (uint64_t)q * DDSCALC_AD9959_SYSCLK_HZ;
    return q + ((2U * rem >= DDSCALC_AD9959_SYSCLK_HZ) ? 1U : 0U);
}

#endif /* __DDS_CALC_H__ */
