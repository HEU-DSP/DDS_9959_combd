# DMA Mode — LPTIM3→DMAMUX→SPI1 DMA Trigger Chain Specification

**日期**：2026-08-08
**分支**：Develop_Basic_Buffered_Operation
**编译开关**：`AD9959_DOWNGRADE_MODE = 0`（`App/phase1_config.h`）

## 背景

原仓库 `AD9959_DOWNGRADE_MODE = 1` 使用 TIM8 10 kHz 自由运行 ISR + 阻塞 `HAL_SPI_Transmit` 发送 AD9959 寄存器帧。本文档定义目标传输模式：**LPTIM3 硬件时基 → DMAMUX 同步门控 → SPI1 DMA → TIM8 ETR 硬件复位**，编码零拷贝，ISR 最小化。

## Hardware Trigger Chain

```
LPTIM3 (PA1 = OUT, internal clock, 135 MHz / DIV1)
  │  LPTIM3_OUT periodic pulse (ARR = 135e6 / rate - 1)
  ├─→ DMAMUX sync gate (SPI1_TX, SyncSignal = HAL_DMAMUX1_SYNC_LPTIM3_OUT,
  │     SyncPolarity = RISING, RequestNumber = 14 bytes per pulse)
  │     └─→ SPI1 DMA TX from ping-pong FrameBank (8-bit DataSize, 125 Mbps)
  │           └─→ PA15 (CS), PB3 (SCLK), PD7 (MOSI/SDIO0)
  ├─→ Fly-wire → PA0 (TIM8_ETR) → TIM8 Slave Reset
  │     └─→ CH1 PWM (PC6) = IO_UPDATE, CH2 PWM (PC7) = DIO3
  │           CCR1/CCR2 calibrated via TIM4 captures
  └─→ Fly-wire → PE0 (TIM4_ETR) → TIM4 Slave Reset
        └─→ CH1 (PB6) = CS↓ capture, CH2 (PB7) = CS↑ capture
```

**时序语义**：每个 LPTIM3_OUT 脉冲解锁恰好一个通道的 14 字节寄存器块
（CSR+CFTW+ACR+CPOW）。DMA 在下一个 sync 边界暂停。4 个脉冲 = 1 帧
（4 通道 56 字节），完全硬件自动，无需 ISR 接力。

**关键硬件约束**：DMAMUX REQNB 为 5 位字段（最大 32），无法一次解锁整帧
（56 字节）。`RequestNumber = 14` 是单通道字节数，天然适配。

## Data Flow (Zero-Copy)

```
Main loop:
  1. SymbolSource_Generate(input_bits) → sym_buf
  2. Encoder_BuildBank_DMA()
       → build_channel_command() 调制计算（与降级模式共享）
       → Encoder_Encode1Bit_Direct() 直接写入 TxBuf_GetIdle()->spi1[] (RAM_D2)
       → 无中间 DDS_EncodedFrame，无 memcpy
  3. 等待 ds.dma_bank_ready 标志（DMA TC ISR 置位）

DMA TC ISR (HAL_SPI_TxCpltCallback):
  1. TxBuf_Swap()
  2. HAL_SPI_Transmit_DMA(active->spi1, tx_bank_bytes)  // 从 RAM_D2 重装
  3. ds.frame_count++
  4. ds.dma_bank_ready = 1  // 通知 main loop 填充 idle bank

硬件（无 CPU 参与）:
  LPTIM3 pulse → DMAMUX 解锁 14B → SPI 移位 → TIM8 ETR 复位 → IO_UPDATE
```

## Memory Layout

```
RAM_D2 (0x30000000, 32KB, DMA1 可访问):
  tx_bank[0].spi1[64]  ← DMA source / CPU write target
  tx_bank[0].spi3[64]  ← 预留（SPI3 双线模式）
  tx_bank[1].spi1[64]  ← ping-pong
  tx_bank[1].spi3[64]  ← 预留
  总计 256 字节（链接脚本 .dma_buffer > RAM_D2）

DTCM (0x20000000, CPU 最快):
  pre_encoded[4]       ← 仅降级模式使用（DMA 模式不使用但保留）
  mod_cfg[4], sym_buf  ← 两种模式共用
```

MPU 已配置全区域 Non-Cacheable，无 cache 一致性问题。

## Key Timing

- SPI 帧：单通道 14 字节 @ 125 Mbps = 14 × 64 ns = **896 ns**
- 4 通道整帧：56 字节 = 3.6 µs（由 4 个 LPTIM3 脉冲驱动）
- LPTIM3 周期：ARR = 135e6/rate − 1；默认 rate = 10 kHz（每通道更新率）→ ARR 13499
- main loop 填充窗口：LPTIM3 周期 − 896 ns；@ 10 kHz 约 99 µs，充裕

## IO_UPDATE / DIO3 延迟校准

- TIM8 由 LPTIM3_OUT（PA0 ETR）复位，CH1/CH2 PWM2 在 CCR1/CCR2 处上升
- **CCR1 必须大于 SPI 突发时间**：14 字节 × 64 ns ≈ 252 ticks @ 281.25 MHz
- 初始值：`P1_CH1_DELAY = 350`，`P1_CH2_DELAY = 360`（`phase1_config.h`）
- 校准：TIM4 CH1/CH2 捕获 CS↓/CS↑（PB6/PB7），计算安全裕量后更新 CCR
- `AD9959_TIM4_AUTOCAL_ENABLE = 0`（手动校准阶段）

## 文件变更清单（2026-08-08 实施）

| 文件 | 变更 |
|:--|:--|
| `Middleware/DDS/dds_encoder.h/.c` | 新增 `ENCODER_FRAME_FLAT_BYTES(14)`、`Encoder_Encode1Bit_Direct()`（零拷贝编码） |
| `App/channel_config.c/h` | 抽取 `build_channel_command()` 共享调制计算；新增 `Encoder_BuildBank_DMA()`（#if !DOWNGRADE） |
| `Bsp/bsp_tim.c/h` | 移除 `extern htim2` + TIM2 函数（#if 0）；移除 `dds_calc.h` 依赖（本地 `BSP_TIM_APB2_TIM_HZ`）；新增 `BSP_LPTIM3_ARRForRate()`；更新 TIM8/TIM4 注释 |
| `Drivers/Trigger/trigger.c/h` | 更新头注释（LPTIM3 链路）；`trigger_setup_dmamux()` 运行时重配 RequestNumber=14；移除 `HAL_LPTIM_MODULE_ENABLED` 条件 |
| `App/debug_state.h` | DebugState 新增 `dma_bank_ready` |
| `Core/Src/main.c` | TIM8 ISR 条件编译；新增 `HAL_SPI_TxCpltCallback`（DMA 模式）；USER CODE 2/WHILE 双模式分支 |
| `App/dds_control.c` | 抽取 `dds_apply_config()` 共享逻辑；dds_apply/dds_stop_output 双模式分支（DMA 用 Trigger_Stop/Restart） |
| `Core/Src/stm32h7xx_it.c` | DMAMUX1_OVR ISR 增加 overrun 诊断计数 |
| `App/phase1_config.h` | 更新模式注释；P1_CH1/CH2_DELAY 调至 350/360 并注释校准要求 |

## 实施中确认的关键结论（2026-08-08）

### 1. DMAMUX RequestNumber 的 RM0468 依据（§17.4.4）

- `DMAMUX_CxCR` 的 `NBREQ[4:0]`（bit 19–23）定义同步模式下每次同步事件后转发的请求数
- **实际请求数 = NBREQ + 1**（1–32），NBREQ 是 5 位字段（0–31）
- HAL 的 `RequestNumber` 参数 = 实际请求数，内部写寄存器时减 1
  （`stm32h7xx_hal_dma_ex.c:436`：`(RequestNumber - 1U) << DMAMUX_CxCR_NBREQ_Pos`）
- 若新同步事件在上一批请求处理完之前到达 → **SOF 过载标志**（DMAMUX1_CSR）→ `DMAMUX1_OVR_IRQHandler`
- 本设计 `RequestNumber=14`（NBREQ=13）：每脉冲恰好解锁一个通道的 14 字节

### 2. DMA 地址自增无需额外配置

`hal_msp.c` 已配置 `MemInc = DMA_MINC_ENABLE`（每字节 +1）、`PeriphInc = DMA_PINC_DISABLE`（SPI_TX 固定）。DMA 的 `M0AR`/`NDTR` 是内部寄存器，穿越 DMAMUX 门控暂停周期不会丢失——第 N 个脉冲从 `buf + 14×(N-1)` 继续读。

### 3. AD9959 接受单 CS 帧背靠背寄存器写（datasheet 原文）

> "After transferring all data bytes per the instruction byte, the communication cycle is completed for that register."
> "At the completion of a communication cycle, the AD9959 serial port controller expects the next set of rising SCLK edges to be the instruction byte for the next communication cycle."

ADI 官方参考代码（`SPI_Master.c`）每个寄存器独立 CS 帧是 GPIO 模拟 SPI 的软件习惯，**不是硬件要求**。DMA 模式 56 字节单 CS 帧合法。

### 4. SPI SSOM（NSS Pulse）必须禁用

CubeMX 默认 `NSS_PULSE_ENABLE = SPI_CFG2_SSOM`：字节间 CS 脉冲会中断 AD9959 多字节寄存器写（CFTW 5B / ACR 4B）。已在 `MX_SPI1_Init` USER CODE 段清除 `SPI_CFG2_SSOM`（两种模式均生效）。**这可能是历史问题"传输稳定但是没波"的根因**——降级模式下 NSSP 同样有害。

### 5. IO_UPDATE 逐通道 vs 每帧一次

- **当前设计**：每个 LPTIM3 脉冲（=每通道 14 字节写完）后 TIM8 ETR 复位 → IO_UPDATE 应用该通道。AD9959 datasheet 明确允许："The I/O update can be sent for each communication cycle or can be sent when all serial operations are complete."
- **不要用 TIM8 ETR 预分频（ETPS=/4）实现"每 4 脉冲一次"**：预分频后无效 ETR 期间计数器自由运行（ARR=0xFFFF，~233 µs 周期），CH1/CH2 PWM 会产生伪 IO_UPDATE——会把 AD9959 寄存器刷成不确定状态。
- 若未来需要 4 通道同步更新（如波束赋形），应使用独立的帧计数器 + 单脉冲 IO_UPDATE 输出。

### 6. make 同秒时间戳问题

`sed` 连续两次修改 `phase1_config.h`（同一秒内）时，make 可能不重新编译依赖文件（时间戳粒度）。**切换编译开关后务必 `make clean` 再编译。**

## Verification

1. **编译验证**：`make -j4` 在 `AD9959_DOWNGRADE_MODE=0` 和 `=1` 两种配置下均通过（切换后 `make clean`，避免 make 同秒时间戳问题）
2. **降级模式回归**：`=1` 编译烧录，确认 99.7 MHz CW 输出不变（注意：SSOM 禁用后降级模式的 CS 行为也变了——每寄存器帧内不再有字节间脉冲，可能修复"没波"问题）
3. **DMA 模式 bring-up**：
   - 飞线 PA1 → PA0 + PA1 → PE0
   - `=0` 编译烧录
   - 示波器探头：PA1 (LPTIM3_OUT)、PB3 (SCLK)、PD7 (MOSI)、PA15 (CS)、PC6 (IO_UPDATE)
   - 验证点：
     a. PA1 周期性脉冲（10 kHz）
     b. SPI 数据与 LPTIM3 脉冲同步（每脉冲 14 字节，CS 全程低）
     c. IO_UPDATE（PC6）在 CS↑ 之后
     d. 4 通道 CW @ 99.7 MHz 正常输出
   - Ozone 监视：`ds.frame_count`、`ds.dma_bank_ready`、`ad9959_diag.spi1_done_count`、`ad9959_diag.dmamux1_csr`（overrun）
4. **模式切换**：编译期决定（宏），不可热切换；分别烧录验证
