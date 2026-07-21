# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Build (GCC arm-none-eabi-)
make                 # outputs: build/DDS_9959_combd.elf, .hex, .bin

# Clean
make clean

# Flash/debug: use J-Link (DDS.jdebug in root) or Keil MDK-ARM
# Keil project: MDK-ARM/DDS_9959_combd.uvprojx
```

Toolchain: `arm-none-eabi-gcc`, target `STM32H723xx` (Cortex-M7, FPv5-D16, hard float ABI). Set `GCC_PATH` env var if the toolchain is not on `PATH`. Keil MDK is an alternative IDE — Makefile and Keil project **must stay in sync** when source files are added/removed.

## Architecture: Software Defined Transmitter (SDT)

This is a multi-purpose digital transmitter built on **STM32H723 + AD9959 DDS**. The system follows strict top-down layering — cross-layer reverse calls are forbidden.

```
App (business logic: tx_manager, cmd_parser, mod_config)
  → Middleware (modulation pipeline: Packet → Symbol → PulseShaping → Modulator → DDS_Command → Encoder)
    → Drivers (external chips: AD9959, 74HC595, APS6404 PSRAM, trigger chain)
      → BSP (MCU peripheral thin wrappers: SPI, TIM, DMA, GPIO, UART)
        → Core (CubeMX HAL init, never put business logic here)
```

### Layer Rules

- **Core/**: CubeMX auto-generated. Only modify inside `USER CODE` blocks. No business logic, no algorithms.
- **Bsp/**: Per-peripheral thin wrappers over HAL. **Must never** contain business symbols (`DDS`, `FSK`, `AD9959`, `Profile`).
- **Drivers/**: One chip/module per directory. Only exposes register/config-level operations, no modulation algorithms.
- **Middleware/**: The core pipeline. Each module (DDS, Buffer, Modulator, Packet, Symbol, Scheduler, PulseShaping) is a separate subdirectory. One modulation type per file in `Middleware/Modulator/`.
- **App/**: Business logic — command parsing, tx management, channel configuration.

### Critical Hardware Pipeline

The DMA pipeline drives AD9959 in **2-wire serial mode** (SDIO0 + SDIO1 only, SDIO2 reserved). **LPTIM3** is the single master trigger:

```
PA1 (LPTIM3_OUT, 135 MHz) ─┬→ DMAMUX Sync Gate → SPI1 (PD7/SDIO0) + SPI3 (PD6/SDIO1) DMA
                            ├→ Fly-wire → PA0 (TIM8_ETR) → TIM8 Reset → IO_UPDATE (PC6) + DIO3 (PC7)
                            └→ Fly-wire → PE0 (TIM4_ETR) → TIM4 counter reset
```

- **LPTIM3_OUT** period match opens DMAMUX sync gate, triggers both SPI DMA streams simultaneously, and resets TIM8/TIM4 counters.
- **TIM4_CH1/CH2** capture CS↓/CS↑ edges on PA15 (SPI1 hardware NSS) to measure DMA-to-SPI latency, used to calibrate TIM8 pulse delays.
- **TIM8** is reset by ETR (was ITR1 from TIM2 in previous revision). CH1/CH2 output compare pulses generate IO_UPDATE and DIO3 signals.
- **Ping-pong buffer**: Two SRAM banks (`tx_bank[0/1]`). DMA reads from the active bank; CPU fills the idle bank from PSRAM. `HAL_SPI_TxCpltCallback` swaps banks and re-arms DMA.
- TIM2 is no longer part of the trigger chain (kept in CubeMX for potential future use).

## Naming & Code Conventions

| Item | Convention | Example |
|------|-----------|---------|
| Functions | `ModuleName_Action()` | `AD9959_Init()`, `HC595_Write()` |
| Structs | `ModuleName_TypeDef` | `AD9959_TypeDef` |
| Enums | `ModuleName_Category` | `Module_Status`, `DDS_WaveType` |
| Macros | `MODULE_UPPER_CASE` | `PID_OUTPUT_MAX`, `FTW_10MHZ` |
| Local variables | `lower_snake_case` with units | `freq_hz`, `voltage_mv` |

- 4-space indentation, Allman brace style (`if\n{\n...\n}`)
- Doxygen `@brief`/`@param`/`@return`/`@note` on all public API functions
- Bare-metal, no RTOS. Interrupts should only set flags or do minimal data movement — no blocking, no floating-point, no serial prints in ISRs.

## Current Development Status (Phase 1–2)

**Working**: CW, FSK, ASK modulation via ping-pong DMA at configurable sample rates. 4-channel AD9959 profile selection. Basic timing measurement (TIM4 captures).

**Skeleton/stubs**: `Drivers/APS6404/aps6404.h` (PSRAM not implemented), `AD9959_ReadRegister()` (not verified), `Test/` directory (empty). Many modulators in the spec (`mod_gfsk.c`, `mod_bpsk.c`, etc.) do not yet exist.

**Known limits**: SDIO2 reserved/unused, FSK/ASK consume 1 symbol per frame, TIM8 delay values are estimates needing calibration, no multi-board sync (FR2 sync defined but unused).

## Key Documentation

- `DOCS/Architecture_Specification.md` — Full architecture, data flow, all 21 sections including power sequence and design principles
- `DOCS/调试手册.md` — Debug manual with signal paths, ISR flow, debugger watch variables
- `DOCS/DSP仓库分层与代码规范.md` — Layer discipline and code conventions (Chinese)
- `DOCS/引脚对照表.md` — Complete pin mapping with DMA timing diagrams

## Development Constraints

- **CubeMX code**: Never modify outside `USER CODE BEGIN/END` blocks — changes will be overwritten on re-generation.
- **Source list**: When adding `.c` files, update both `Makefile` (`C_SOURCES`) and the Keil project (`MDK-ARM/DDS_9959_combd.uvprojx`).
- **Build directory**: `build/` is git-ignored.
- **BSP purity**: Files under `Bsp/` must not reference DDS, FSK, AD9959, or any application/middleware types.
- **Modulator interface**: All modulators should follow `Modulator_Process(const Symbol *sym, DDS_Command *cmd)` and `Modulator_Init(const ModConfig *cfg)`.
