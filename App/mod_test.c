/**
 ******************************************************************************
 * @file    mod_test.c
 * @brief   Modulation sweep test using the existing mod_cfg + SymbolBuffer path
 ******************************************************************************
 */

#include "mod_test.h"
#include "main.h"
#include "mod_config.h"
#include "phase1_config.h"
#include "symbol_buffer.h"

#ifndef AD9959_MOD_TEST_ENABLE
#define AD9959_MOD_TEST_ENABLE  0U
#endif

#ifndef AD9959_MOD_TEST_CHANNEL
#define AD9959_MOD_TEST_CHANNEL  1U
#endif

#ifndef AD9959_MOD_TEST_HOLD_MS
#define AD9959_MOD_TEST_HOLD_MS  3000U
#endif

#define MOD_TEST_MODE_COUNT  10U

volatile ModTestControl mod_test_ctrl = {
    .enable = AD9959_MOD_TEST_ENABLE,
    .auto_advance = 0U,
    .selected_index = 0U,
    .hold_ms = AD9959_MOD_TEST_HOLD_MS,
    .force_apply = 1U,
};

volatile ModTestDebug mod_test_debug = {0};

static uint8_t test_symbols[SYM_BUF_CAPACITY];
static uint32_t last_applied_index = 0xFFFFFFFFUL;
static uint32_t last_requested_index = 0xFFFFFFFFUL;
static uint32_t last_switch_tick = 0U;
static volatile uint32_t pending_index = 0U;
static volatile uint32_t pending_valid = 0U;

static uint32_t ftw_add(uint32_t base, uint32_t delta)
{
    return base + delta;
}

static void fill_symbol_pattern(void)
{
    for (uint16_t i = 0U; i < SYM_BUF_CAPACITY; i++) {
        test_symbols[i] = (uint8_t)(i & 0x01U);
    }
}

static void prime_symbols_locked(void)
{
    SymbolBuf_Clear();

    if (mod_test_debug.bits_per_symbol != 0U) {
        if (SymbolBuf_WriteBits(test_symbols, SYM_BUF_CAPACITY)) {
            mod_test_debug.symbol_count = SYM_BUF_CAPACITY;
            mod_test_debug.feed_count++;
        }
    } else {
        mod_test_debug.symbol_count = 0U;
    }
}

static void disable_other_channels(uint8_t active_ch)
{
    for (uint8_t ch = 0U; ch < DDS_CHANNEL_COUNT; ch++) {
        if (ch != active_ch) {
            ModCfg_Disable(ch);
        }
    }
}

static void apply_mode(uint32_t index)
{
    uint8_t ch = (uint8_t)AD9959_MOD_TEST_CHANNEL;
    uint32_t f0 = FTW_100P3MHZ;
    uint32_t f1 = ftw_add(FTW_100P3MHZ, FTW_5KHZ);
    uint32_t f2 = ftw_add(FTW_100P3MHZ, FTW_5KHZ * 2UL);
    uint32_t f3 = ftw_add(FTW_100P3MHZ, FTW_5KHZ * 3UL);
    uint32_t primask;

    if (ch >= DDS_CHANNEL_COUNT) {
        ch = 1U;
    }

    primask = __get_PRIMASK();
    __disable_irq();

    disable_other_channels(ch);

    switch (index % MOD_TEST_MODE_COUNT) {
    case 0U:
        ModCfg_SetCW(ch, f0 + FTW_100KHZ, 0x03FFU);
        mod_test_debug.bits_per_symbol = 0U;
        mod_test_debug.ftw0 = f0;
        mod_test_debug.ftw1 = f0;
        mod_test_debug.asf0 = 0x03FFU;
        mod_test_debug.asf1 = 0x03FFU;
        break;

    case 1U:
        ModCfg_SetFSK(ch, f1, f0, 0x03FFU);
        mod_test_debug.bits_per_symbol = 1U;
        mod_test_debug.ftw0 = f0;
        mod_test_debug.ftw1 = f1;
        mod_test_debug.asf0 = 0x03FFU;
        mod_test_debug.asf1 = 0x03FFU;
        break;

    case 2U:
        ModCfg_SetASK(ch, f0, 0x03FFU, 0x0080U);
        mod_test_debug.bits_per_symbol = 1U;
        mod_test_debug.ftw0 = f0;
        mod_test_debug.ftw1 = f0;
        mod_test_debug.asf0 = 0x0080U;
        mod_test_debug.asf1 = 0x03FFU;
        break;

    case 3U:
        ModCfg_SetGFSK(ch, f1, (int32_t)FTW_10MHZ, 0x03FFU);
        mod_test_debug.bits_per_symbol = 1U;
        mod_test_debug.ftw0 = f0;
        mod_test_debug.ftw1 = f2;
        mod_test_debug.asf0 = 0x03FFU;
        mod_test_debug.asf1 = 0x03FFU;
        break;

    case 4U:
        ModCfg_SetMSK(ch, f1, (int32_t)FTW_10MHZ, 0x03FFU, 0x0100U);
        mod_test_debug.bits_per_symbol = 1U;
        mod_test_debug.ftw0 = f0;
        mod_test_debug.ftw1 = f2;
        mod_test_debug.asf0 = 0x03FFU;
        mod_test_debug.asf1 = 0x03FFU;
        break;

    case 5U:
        ModCfg_SetQPSK(ch, f0, 0x03FFU);
        mod_test_debug.bits_per_symbol = 2U;
        mod_test_debug.ftw0 = f0;
        mod_test_debug.ftw1 = f0;
        mod_test_debug.asf0 = 0x03FFU;
        mod_test_debug.asf1 = 0x03FFU;
        break;

    case 6U:
        ModCfg_SetAM(ch, f0, 0x0240U, 0x01C0U);
        mod_test_debug.bits_per_symbol = 1U;
        mod_test_debug.ftw0 = f0;
        mod_test_debug.ftw1 = f0;
        mod_test_debug.asf0 = 0x0080U;
        mod_test_debug.asf1 = 0x0400U;
        break;

    case 7U:
        ModCfg_SetFM(ch, f1, (int32_t)FTW_10MHZ, 0x03FFU);
        mod_test_debug.bits_per_symbol = 1U;
        mod_test_debug.ftw0 = f0;
        mod_test_debug.ftw1 = f2;
        mod_test_debug.asf0 = 0x03FFU;
        mod_test_debug.asf1 = 0x03FFU;
        break;

    case 8U:
        ModCfg_SetBPSK(ch, f0, 0x03FFU, 0x0000U, 0x2000U);
        mod_test_debug.bits_per_symbol = 1U;
        mod_test_debug.ftw0 = f0;
        mod_test_debug.ftw1 = f0;
        mod_test_debug.asf0 = 0x03FFU;
        mod_test_debug.asf1 = 0x03FFU;
        break;

    default:
        ModCfg_Set4FSK(ch, f0, f1, f2, f3, 0x03FFU);
        mod_test_debug.bits_per_symbol = 2U;
        mod_test_debug.ftw0 = f0;
        mod_test_debug.ftw1 = f3;
        mod_test_debug.asf0 = 0x03FFU;
        mod_test_debug.asf1 = 0x03FFU;
        break;
    }

    prime_symbols_locked();

    if (primask == 0U) {
        __enable_irq();
    }

    mod_test_debug.active_index = index % MOD_TEST_MODE_COUNT;
    mod_test_debug.active_mode = mod_cfg[ch].mode;
    mod_test_debug.active_channel = ch;
    mod_test_debug.apply_count++;
}

static void feed_symbols_if_needed(void)
{
    if (mod_test_debug.bits_per_symbol == 0U) {
        return;
    }

    if (SymbolBuf_IsFree()) {
        if (SymbolBuf_WriteBits(test_symbols, SYM_BUF_CAPACITY)) {
            mod_test_debug.symbol_count = SYM_BUF_CAPACITY;
            mod_test_debug.feed_count++;
        }
    } else {
        mod_test_debug.symbol_count = SymbolBuf_Count();
    }
}

void ModTest_Init(uint32_t now_ms)
{
    fill_symbol_pattern();
    last_switch_tick = now_ms;

    if (mod_test_ctrl.enable != 0U) {
        mod_test_ctrl.selected_index %= MOD_TEST_MODE_COUNT;
        apply_mode(mod_test_ctrl.selected_index);
        last_applied_index = mod_test_ctrl.selected_index;
        last_requested_index = mod_test_ctrl.selected_index;
        mod_test_ctrl.force_apply = 0U;
        feed_symbols_if_needed();
    }
}

void ModTest_Task(uint32_t now_ms)
{
    uint32_t index;
    uint32_t hold_ms;

    if (mod_test_ctrl.enable == 0U) {
        return;
    }

    hold_ms = mod_test_ctrl.hold_ms;
    if (hold_ms == 0U) {
        hold_ms = AD9959_MOD_TEST_HOLD_MS;
    }

    index = mod_test_ctrl.selected_index % MOD_TEST_MODE_COUNT;
    if ((mod_test_ctrl.auto_advance != 0U) &&
        ((uint32_t)(now_ms - last_switch_tick) >= hold_ms)) {
        index = (mod_test_debug.active_index + 1U) % MOD_TEST_MODE_COUNT;
        mod_test_ctrl.selected_index = index;
        last_switch_tick = now_ms;
    }

    if ((mod_test_ctrl.force_apply != 0U) || (index != last_requested_index)) {
        pending_index = index;
        pending_valid = 1U;
        mod_test_debug.switch_pending = 1U;
        last_requested_index = index;
        mod_test_ctrl.force_apply = 0U;
    }

    feed_symbols_if_needed();
    mod_test_debug.last_tick = now_ms;
}

bool ModTest_HasPendingSwitch(void)
{
    return pending_valid != 0U;
}

bool ModTest_ApplyPendingSwitch(void)
{
    uint32_t index;
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    if (pending_valid == 0U) {
        if (primask == 0U) {
            __enable_irq();
        }
        return false;
    }

    index = pending_index;
    pending_valid = 0U;
    if (primask == 0U) {
        __enable_irq();
    }

    /* Called only after the SPI1 completion ISR has paused the trigger
     * chain.  No DMA engine is reading either ping-pong bank here. */
    apply_mode(index);
    last_applied_index = index;
    mod_test_debug.switch_pending = 0U;
    mod_test_debug.switch_count++;
    return true;
}
