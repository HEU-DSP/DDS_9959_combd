/**
 ******************************************************************************
 * @file    ad9959_reg.h
 * @brief   AD9959 Complete Register Map & Bit Definitions
 *
 * Reference: AD9959 Data Sheet Rev. B (2008), Tables 28–33 + footnotes
 *
 * SPI Instruction Byte:
 *   bit[7]    = R/W_n: 0 = Write, 1 = Read
 *   bits[6:0] = Register Address (0x00–0x09 control, 0x0A–0x18 profile)
 *
 * Channel access: CSR[7:4] selects which channel(s) receive register writes.
 * All channel registers share the same address per type (CFTW=0x04 for all ch).
 ******************************************************************************
 */

#ifndef __AD9959_REG_H__
#define __AD9959_REG_H__

/* ================================================================
 * 1. Register Address Map (Table 28–30)
 * ================================================================ */

/* ---- Table 28: Control Registers ---- */
#define AD9959_REG_CSR       0x00   /* Channel Select Register         (1 byte)  */
#define AD9959_REG_FR1       0x01   /* Function Register 1              (3 bytes) */
#define AD9959_REG_FR2       0x02   /* Function Register 2              (2 bytes) */

/* ---- Table 29: Channel Registers (CSR[7:4] selects target channel) ---- */
#define AD9959_REG_CFR       0x03   /* Channel Function Register        (3 bytes) */
#define AD9959_REG_CFTW      0x04   /* Channel Frequency Tuning Word    (4 bytes) */
#define AD9959_REG_CPOW      0x05   /* Channel Phase Offset Word        (2 bytes) */
#define AD9959_REG_ACR       0x06   /* Amplitude Control Register       (3 bytes) */
#define AD9959_REG_LSRR      0x07   /* Linear Sweep Ramp Rate           (2 bytes) */
#define AD9959_REG_RDW       0x08   /* LSR Rising Delta Word            (4 bytes) */
#define AD9959_REG_FDW       0x09   /* LSR Falling Delta Word           (4 bytes) */

/* ---- Table 30: Profile (Channel Word) Registers ---- */
#define AD9959_REG_CW1       0x0A
#define AD9959_REG_CW2       0x0B
#define AD9959_REG_CW3       0x0C
#define AD9959_REG_CW4       0x0D
#define AD9959_REG_CW5       0x0E
#define AD9959_REG_CW6       0x0F
#define AD9959_REG_CW7       0x10
#define AD9959_REG_CW8       0x11
#define AD9959_REG_CW9       0x12
#define AD9959_REG_CW10      0x13
#define AD9959_REG_CW11      0x14
#define AD9959_REG_CW12      0x15
#define AD9959_REG_CW13      0x16
#define AD9959_REG_CW14      0x17
#define AD9959_REG_CW15      0x18

/* ================================================================
 * 2. CSR (0x00) — Channel Select Register [7:0]  (Table 31)
 *
 *   [7:4] Channel enable — active IMMEDIATELY (no I/O update needed).
 *         Write 1 per channel to enable.  Default = 0xF (all enabled).
 *         Example: 0x10 = only CH0, 0x20 = only CH1, 0xF0 = all four.
 *   [3]   Must be 0.
 *   [2:1] Serial I/O mode:
 *           00 = single-bit 2-wire   (SDIO_0 only)
 *           01 = single-bit 3-wire   (SDIO_0 + SDIO_1 output + SDIO_2 in/out)
 *           10 = 2-bit serial mode   (SDIO_0 + SDIO_1)
 *           11 = 4-bit serial mode   (SDIO_0–SDIO_3)
 *   [0]   LSB first: 0 = MSB first (default), 1 = LSB first.
 * ================================================================ */
#define CSR_CH0_ENABLE       (1U << 4)
#define CSR_CH1_ENABLE       (1U << 5)
#define CSR_CH2_ENABLE       (1U << 6)
#define CSR_CH3_ENABLE       (1U << 7)
#define CSR_CHANNEL_MASK     0xF0U
#define CSR_CHANNEL(ch)      (1U << ((ch) + 4))   /* ch 0–3 → CSR bit */

#define CSR_IO_MODE_Pos      1
#define CSR_IO_MODE_Msk      0x06U
#define CSR_IO_MODE_2WIRE    (0x00U << CSR_IO_MODE_Pos)
#define CSR_IO_MODE_3WIRE    (0x01U << CSR_IO_MODE_Pos)
#define CSR_IO_MODE_2BIT     (0x02U << CSR_IO_MODE_Pos)
#define CSR_IO_MODE_4BIT     (0x03U << CSR_IO_MODE_Pos)

#define CSR_LSB_FIRST        (1U << 0)

/* ================================================================
 * 3. FR1 (0x01) — Function Register 1 [23:0]  (Table 32)
 *
 *   [23]    VCO gain control:
 *             0 = low range  (SYSCLK <  160 MHz) (default)
 *             1 = high range (SYSCLK >  255 MHz)
 *   [22:18] PLL divider ratio: 4–20 = PLL multiplier.
 *             Values outside 4–20 disable the PLL.
 *   [17:16] Charge pump control:
 *             00 =  75 µA (default)
 *             01 = 100 µA
 *             10 = 125 µA
 *             11 = 150 µA
 *   [15]    Open (reserved).
 *   [14:12] Profile pin configuration (PPC).
 *   [11:10] Ramp-up / ramp-down (RU/RD).
 *   [9:8]   Modulation level (2/4/8/16-level).
 *   [7]     Reference clock input power-down:
 *             0 = enabled (default), 1 = powered down.
 *   [6]     External power-down mode:
 *             0 = fast recovery (default), 1 = full power-down.
 *   [5]     SYNC_CLK disable:
 *             0 = active (default), 1 = disabled (static 0).
 *   [4]     DAC reference power-down:
 *             0 = enabled (default), 1 = powered down.
 *   [3:2]   Open (reserved).
 *   [1]     Manual hardware sync.
 *   [0]     Manual software sync.
 * ================================================================ */

/* VCO gain [23] */
#define FR1_VCO_GAIN_Pos         23
#define FR1_VCO_GAIN_Msk         (1UL << FR1_VCO_GAIN_Pos)
#define FR1_VCO_GAIN_LOW         (0UL << FR1_VCO_GAIN_Pos)   /* SYSCLK < 160 MHz   */
#define FR1_VCO_GAIN_HIGH        (1UL << FR1_VCO_GAIN_Pos)   /* SYSCLK > 255 MHz   */

/* PLL divider ratio [22:18] — value = multiplication factor (4–20) */
#define FR1_PLL_DIV_Pos          18
#define FR1_PLL_DIV_Msk          (0x1FUL << FR1_PLL_DIV_Pos)
#define FR1_PLL_DIV(n)           (((n) & 0x1FUL) << FR1_PLL_DIV_Pos)

/* Charge pump [17:16] */
#define FR1_CHARGE_PUMP_Pos      16
#define FR1_CHARGE_PUMP_Msk      (0x03UL << FR1_CHARGE_PUMP_Pos)
#define FR1_CP_75uA              (0x00UL << FR1_CHARGE_PUMP_Pos)
#define FR1_CP_100uA             (0x01UL << FR1_CHARGE_PUMP_Pos)
#define FR1_CP_125uA             (0x02UL << FR1_CHARGE_PUMP_Pos)
#define FR1_CP_150uA             (0x03UL << FR1_CHARGE_PUMP_Pos)

/* Modulation level [9:8] */
#define FR1_MOD_LEVEL_Pos        8
#define FR1_MOD_LEVEL_Msk        (0x03UL << FR1_MOD_LEVEL_Pos)

/* Bit masks */
#define FR1_REFCLK_PWRDN         (1UL << 7)
#define FR1_EXT_PWRDN_MODE       (1UL << 6)
#define FR1_SYNCCLK_DISABLE      (1UL << 5)
#define FR1_DAC_REF_PWRDN        (1UL << 4)
#define FR1_MANUAL_HW_SYNC       (1UL << 1)
#define FR1_MANUAL_SW_SYNC       (1UL << 0)

/* ================================================================
 * 4. FR2 (0x02) — Function Register 2 [15:0]  (Table 33)
 *
 *   [15]    All channels autoclear sweep accumulator.
 *   [14]    All channels clear sweep accumulator.
 *   [13]    All channels autoclear phase accumulator.
 *   [12]    All channels clear phase accumulator.
 *   [11:10] Open.
 *   [9]     Auto sync enable.
 *   [8]     Multidevice sync master enable.
 *   [7]     Multidevice sync status (read-only).
 *   [6]     Multidevice sync mask.
 *   [5:4]   Open.
 *   [3:2]   Open.
 *   [1:0]   System clock offset.
 * ================================================================ */
#define FR2_ALL_AUTOCLR_SWEEP   (1U << 15)
#define FR2_ALL_CLR_SWEEP       (1U << 14)
#define FR2_ALL_AUTOCLR_PHASE   (1U << 13)
#define FR2_ALL_CLR_PHASE       (1U << 12)
#define FR2_AUTO_SYNC_ENABLE    (1U << 9)
#define FR2_MULTIDEV_SYNC_MASTER (1U << 8)
#define FR2_MULTIDEV_SYNC_STATUS (1U << 7)
#define FR2_MULTIDEV_SYNC_MASK   (1U << 6)
#define FR2_SYSCLK_OFFSET_Pos    0
#define FR2_SYSCLK_OFFSET_Msk    0x03U

/* ================================================================
 * 5. CFR (0x03) — Channel Function Register [23:0]  (Table 29)
 *
 *   [23:22] AFP select  (Amplitude / Frequency / Phase select).
 *           Effectively the channel identifier for this CFR write.
 *   [21:16] Open.
 *   [15]    Load SRR at I/O_UPDATE.
 *   [14]    Linear sweep no-dwell.
 *   [13]    Linear sweep enable.
 *   [12:11] Open.
 *   [10]    Matched pipe delays active.
 *   [9:8]   DAC full-scale current control (must be 11b = max).
 *   [7]     Digital power-down.
 *   [6]     DAC power-down.
 *   [5]     Autoclear sweep accumulator.
 *   [4]     Clear sweep accumulator.
 *   [3]     Autoclear phase accumulator.
 *   [2]     Clear phase accumulator 2.
 *   [1]     Sine wave output enable.
 *   [0]     Must be 0.
 * ================================================================ */

/* AFP Select [23:22] */
#define CFR_AFP_Pos             22
#define CFR_AFP_Msk             (0x03UL << CFR_AFP_Pos)
#define CFR_AFP_CH0             (0x00UL << CFR_AFP_Pos)
#define CFR_AFP_CH1             (0x01UL << CFR_AFP_Pos)
#define CFR_AFP_CH2             (0x02UL << CFR_AFP_Pos)
#define CFR_AFP_CH3             (0x03UL << CFR_AFP_Pos)

#define CFR_LOAD_SRR_IOUPDATE   (1UL << 15)
#define CFR_LSWEEP_NO_DWELL     (1UL << 14)
#define CFR_LSWEEP_ENABLE       (1UL << 13)
#define CFR_MATCHED_PIPE_DELAYS (1UL << 10)
#define CFR_DAC_FULL_CURRENT    (0x03UL << 8)  /* Must be 11b */
#define CFR_DIGITAL_PWRDN       (1UL << 7)
#define CFR_DAC_PWRDN           (1UL << 6)
#define CFR_AUTOCLR_SWEEP       (1UL << 5)
#define CFR_CLR_SWEEP           (1UL << 4)
#define CFR_AUTOCLR_PHASE       (1UL << 3)
#define CFR_CLR_PHASE_ACC2      (1UL << 2)
#define CFR_SINE_OUT_ENABLE     (1UL << 1)

/* ================================================================
 * 6. CFTW (0x04) — Channel Frequency Tuning Word [31:0]  (4 bytes)
 *
 *   FTW = f_OUT / f_SYSCLK × 2^32
 *   CSR[7:4] selects which channel(s) receive the value.
 * ================================================================ */

/* ================================================================
 * 7. CPOW (0x05) — Channel Phase Offset Word [15:0]  (2 bytes)
 *
 *   [15:14] Open (unused).
 *   [13:0]  Phase Offset Word: offset = POW / 2^14 × 360°
 *   CSR[7:4] selects which channel(s) receive the value.
 * ================================================================ */
#define CPOW_POS                0
#define CPOW_MASK               0x3FFFU      /* Bits [13:0] */
#define CPOW_MAX                16383

/* ================================================================
 * 8. ACR (0x06) — Amplitude Control Register [23:0]  (3 bytes)
 *
 *   [23:16] Amplitude ramp rate.
 *   [15]    Open.
 *   [12]    Amplitude multiplier enable:
 *             0 = bypass (ASF ignored), 1 = enabled (ASF controls amplitude).
 *   [13]    Ramp-up/ramp-down enable (auto RU/RD).
 *   [12]    Load ARR at I/O_UPDATE.
 *   [11]    Open.
 *   [10]    Open.
 *   [9:0]   Amplitude Scale Factor (10-bit): 0 = off, 0x3FF = full scale.
 *   CSR[7:4] selects which channel(s) receive the value.
 * ================================================================ */
#define ACR_AMP_MULT_ENABLE     (1U << 12)
#define ACR_AUTO_RAMP_ENABLE    (1U << 11)
#define ACR_LOAD_ARR_IOUPDATE   (1U << 10)
#define ACR_ASF_Pos             0
#define ACR_ASF_Msk             0x3FFU
#define ACR_ASF_MAX             0x3FFU
#define ACR_ASF_MIN             0x000U

/* ================================================================
 * 9. LSRR (0x07) — Linear Sweep Ramp Rate [15:0]  (2 bytes)
 *
 *   [15:8]  Falling sweep ramp rate (FSRR).
 *   [7:0]   Rising sweep ramp rate (RSRR).
 * ================================================================ */

/* ================================================================
 * 10. RDW (0x08) — LSR Rising Delta Word [31:0]  (4 bytes)
 *
 *   Frequency increment per SYNC_CLK cycle during upward sweep.
 * ================================================================ */

/* ================================================================
 * 11. FDW (0x09) — LSR Falling Delta Word [31:0]  (4 bytes)
 *
 *   Frequency decrement per SYNC_CLK cycle during downward sweep.
 * ================================================================ */

/* ================================================================
 * 12. CW1–CW15 (0x0A–0x18) — Profile (Channel Word) Registers
 *
 *   32-bit per word (accessed as 4 bytes at the given address).
 *   CSR[7:4] selects which channel's profile bank is accessed.
 *   Content interpretation depends on modulation mode:
 *     Frequency word:  FTW [31:0]
 *     Phase word:      POW [31:18] (MSB-aligned)
 *     Amplitude word:  ASF [31:22] (MSB-aligned)
 * ================================================================ */

#endif /* __AD9959_REG_H__ */
