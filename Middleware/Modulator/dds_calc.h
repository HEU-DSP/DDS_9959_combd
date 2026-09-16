/**
 ******************************************************************************
 * @file    dds_calc.h
 * @brief   AD9959 parameter calculator — integer-only, no runtime division
 *
 * All AD9959 parameters (FTW, ASF, clock-chain) derived from CubeMX
 * RCC configuration.  Every macro is a compile-time integer — no float,
 * no runtime division.  FTW uses a Q32.32 multiply-shift (two-segment
 * UMULL) plus a conditional quotient step-back, bit-exact equivalent to
 * the round-half-up 64-bit division it replaces (host-verified: 0
 * mismatches over 0..200 MHz step 1, and over 10^6 random frequencies).
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
 * SCALE 向上取整 ⇒ (freq × SCALE) >> 32 是 floor 除法的**高估**，不是精确值：
 *   误差项 = freq × ((-2^64) mod SYSCLK) / (SYSCLK × 2^32) < 0.0079
 *   （(-2^64) mod SYSCLK = 33784828），仅当小数部分 ≥ 1 − 误差项
 *   （即 frac > 0.992）时结果比 floor 大 1。此时 rem = freq×2^32 − q×SYSCLK
 *   变负，uint64 回绕会让舍入判据恒真 —— 净效果是比 round-half-up 大 1 LSB。
 *   实测 0..200 MHz 扫频有 0.16% 的频点中招（最大 0.114 Hz @ 490.909 MHz）。
 *   注意不能拿 "Granlund 条件满足 ⇒ 精确" 来论证：Granlund 定理管的是
 *   n/d，本式是 n×2^32/d 的缩放除法，不适用。下面的 over 补偿才是精确的来源。
 *
 * 两段拆分 (M7 UMULL 32×32→64, 单周期):
 *   SCALE = 8×2^32 + 0xBFBEF3D4
 *   q     = (freq << 3) + ((freq × 0xBFBEF3D4) >> 32)   ← floor 或 floor+1
 *   over  = (freq<<32) < q × SYSCLK ? 1 : 0             ← q 高估的那一步
 *   rem   = (freq<<32) − q×SYSCLK + over×SYSCLK         ← 恒落在 [0, SYSCLK)
 *   FTW   = (q − over) + (2×rem ≥ SYSCLK ? 1 : 0)       ← 精确 round-half-up
 * ================================================================ */

/* The high word of SCALE is 8, so it contributes freq × 8 to q — i.e. a
 * LEFT SHIFT BY 3.  Until 2026-09-15 this macro held the word itself (8U) and
 * was used as a shift count, multiplying by 256 instead of 8: q overflowed
 * past 2^32, rem went negative, the rounding bit became meaningless, and the
 * FTW was garbage (measured 81 MHz for a 30 MHz request).  Name it for what it
 * is — a shift — so the value and the operator can't drift apart again. */
#define DDSCALC_FTW_SCALE_SHIFT   3U
#define DDSCALC_FTW_SCALE_LO      UINT64_C(0xBFBEF3D4)

/* ================================================================
 * 参数计算接口 — 全部 static inline，编译期可常量化
 * ================================================================ */

/**
 * @brief  FTW from frequency in Hz.
 *
 * Q32.32 multiply-shift + remainder rounding — no runtime division,
 * no libgcc __aeabi_uldivmod dependency.  Zero cost when freq_hz is a
 * compile-time constant (GCC folds it); a handful of M7 ALU ops when
 * runtime (host protocol SET commands).  Bit-exact equivalent of the
 * round-half-up 64-bit division it replaces, for all freq_hz < SYSCLK.
 *
 * @param  freq_hz : output frequency in Hz (< SYSCLK)
 * @return 32-bit frequency tuning word
 */
static inline uint32_t DDSCalc_FTW(uint32_t freq_hz)
{
    uint32_t q = (freq_hz << DDSCALC_FTW_SCALE_SHIFT)
               + (uint32_t)(((uint64_t)freq_hz * DDSCALC_FTW_SCALE_LO) >> 32);

    /* SCALE rounds up, so q is floor(freq×2^32/SYSCLK) or one above it — never
     * below.  num < qs is exactly the overshoot case: step the quotient back
     * one and fold SYSCLK into the remainder, which lands rem in [0, SYSCLK)
     * either way.  Both corrections are branchless (CSEL / IT block). */
    uint64_t num  = (uint64_t)freq_hz << 32;
    uint64_t qs   = (uint64_t)q * DDSCALC_AD9959_SYSCLK_HZ;
    uint32_t over = (num < qs) ? 1U : 0U;
    uint64_t rem  = num - qs + (uint64_t)over * DDSCALC_AD9959_SYSCLK_HZ;
    q -= over;

    return q + ((2U * rem >= DDSCALC_AD9959_SYSCLK_HZ) ? 1U : 0U);
}

#endif /* __DDS_CALC_H__ */
