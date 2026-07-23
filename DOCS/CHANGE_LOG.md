# Change Log — HW081 AD9959 调试 (2026-07-23)

> 基于 commit `50ee2b2`，分支 `Temp_HW081_Modify`

## 概览

将 74HC595 驱动从 `hc595.c/h` 替换为 `drv_74hc595_1.c/h`，修正 AD9959 上电时序、SPI 配置和复位极性，增加超时检测框架和调试功能。

## 修改文件 (16 modified + 2 new)

### 1. 74HC595 驱动 — `Drivers/595/drv_74hc595_1.{c,h}` (新)

- 位定义严格对齐 `DOCS/引脚对照表.md`：Q0=NC, Q1=LEDSTBY, ..., Q7=LEDCH3TRANSMIT
- 引脚使用 `main.h` CubeMX 宏 (PE2~PE6)
- 两片级联 595：Chip1=LED 指示灯[7:0], Chip2=DDS 控制[15:8]
- API: `DRV_595_Init/Write/SetBit/EnableOutput/Reset/Refresh`
- `PowerSeq_Init()`: 电源轨上电时拉高 MASTER_RESET 保护 DDS
- `Refresh()`: 调试器修改 `g_595.bits.*` 后 100ms 内同步到硬件
- `ShiftReg_State_t` 联合体：命名位段 `l_stby/d_1v8d/...` 便于调试器 watch

### 2. AD9959 驱动 — `Drivers/AD9959/ad9959.{c,h}`

- **SYSCLK = 486.4 MHz** (REFCLK 25.6 MHz × PLL 19)，≤500 MHz 数据手册上限
- Charge pump: 75 μA (DS 推荐最佳相位噪声)
- CFR: 添加 `CFR_MATCHED_PIPE_DELAYS` (single-tone 模式)
- 上电时序: `power rails → 1500ms 稳定 → reset(200ms) → 500ms 恢复 → PLL → CSR`
- MASTER_RESET 极性修正为 **Active HIGH** (DS p9)
- `AD9959_Debug_CW_Test(freq_hz)`: 配置 CH0 CW 输出，最大幅度
- `AD9959_ConfigPLL()`: FR1 写入后立即 IO_UPDATE 启动 PLL
- `WriteRegister()`: 单线模式下仅使用 SPI1 (SDIO_0)

### 3. 超时检测框架 — `App/detect_task.{c,h}`

- 从其他工程移植，提供 `Detect_Hook/Detect_Task/is_TOE_Overtime`
- `errorlist` 新增 4 个 DDS 上电超时条目: STABLE(1500ms), HOLD(200ms), RECOVERY(500ms), LOCK(100ms)
- 替代 `HAL_Delay()`，支持调试器断点中断等待
- `Detect_Init()` 在 main 初始化早期调用，`Detect_Task()` 在 while 循环

### 4. 系统时钟 — `Core/Src/main.c`, `App/phase1_config.h`

- HSE = 12 MHz
- PLL1: M=3, N=128, P=1 → VCO=512 MHz, SYSCLK=512 MHz, HCLK=256 MHz
- TIM15 (REFCLK): 256 MHz → ARR=9 → 25.6 MHz 50% duty
- `BSP_TIM15_SetREFCLK()` 改用 `HAL_RCC_GetHCLKFreq()` 替代硬编码 135 MHz

### 5. SPI 配置 — `Core/Src/main.c`

- **DataSize 4-bit → 8-bit**：4-bit 模式下每帧仅 4 位，且 NSS 脉冲间复位 AD9959 串口，导致寄存器写入失败
- 移除 USART10 初始化 (595 改用 GPIO 位操作)
- PLL2 参数修正: M=4, N=160, Q=4

### 6. 调试主循环 — `Core/Src/main.c`

- 每 ~200ms 循环：`AD9959_Reset() → ConfigPLL() → Debug_CW_Test(200MHz)`
- 便于示波器捕获 SPI/IO_UPDATE 信号
- 原有 DMA/Encoder/Trigger 代码注释保留

### 7. 构建系统 — `Makefile`

- 添加 `App/detect_task.c`
- `Drivers/595/hc595.c` → `Drivers/595/drv_74hc595_1.c`

### 8. BSP 层 — `Bsp/bsp_tim.c`, `Bsp/bsp_gpio.{c,h}`

- `BSP_TIM15_SetREFCLK()`: 动态获取 HCLK，修正占空比计算 `(arr+1)/2`
- 移除旧 74HC595 GPIO 封装 (已由新驱动直接使用 HAL)

## 当前状态

- ✅ 74HC595 输出正常 (LED/DDS 控制位正确)
- ✅ REFCLK 25.6 MHz 输出正常
- ⬜ AD9959 SPI 寄存器写入验证中 (周期性复位+重配置循环运行中)
- ⬜ CH0 200 MHz RF 输出待验证
