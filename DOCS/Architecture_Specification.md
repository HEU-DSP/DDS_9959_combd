# STM32H723 + AD9959 多用途数字发射机软件架构设计

**Version：0.2**

> 硬件引脚详情参见：[引脚对照表](./引脚对照表.md)

---

# 1 项目目标

## 1.1 项目简介

本项目旨在构建一个基于 **STM32H723 + AD9959 DDS** 的多用途数字发射机软件平台。

系统采用模块化设计，使其能够支持多种数字调制方式，并具有良好的可扩展性。

设计目标包括：

- CW
- ASK/OOK
- FSK
- GFSK
- MSK
- BPSK
- QPSK
- MFSK
- AM
- FM

未来可进一步扩展：

- OFDM
- DSSS
- FHSS
- IQ 波形回放

系统整体遵循**软件定义发射机（Software Defined Transmitter，SDT）**思想，将调制算法、DDS 驱动和硬件控制解耦。

---

# 2 软件总体架构

整个软件采用自上而下的分层结构。

```
                Host Interface
                      │
                      ▼
              Command Parser
                      │
                      ▼
                Tx Manager
                      │
            ┌─────────┴─────────┐
            ▼                   ▼
     Packet Generator       Configuration
            │
            ▼
     Symbol Generator
            │
            ▼
      Pulse Shaping
            │
            ▼
    DDS Command Generator
            │
            ▼
      AD9959 Encoder
            │
            ▼
       DMA Scheduler
            │
            ▼
    SPI DMA → AD9959 → IO_UPDATE
```

软件分层之间仅允许通过接口通信，不允许跨层直接访问内部数据。

---

# 3 软件目录结构

```
Project
│
├── Core/                   # CubeMX 自动生成，HAL 初始化代码
│   ├── Inc/                #   外设初始化头文件 (main.h, spi.h, tim.h, ...)
│   ├── Src/                #   外设初始化源文件 (main.c, stm32h7xx_it.c, ...)
│   └── Startup/            #   启动文件
│
├── BSP/                    # 板级支持包，MCU 外设抽象
│   ├── bsp_spi.c/h         #   SPI1/SPI3 发送与 DMA 接口
│   ├── bsp_tim.c/h         #   TIM2/4/8/15 控制接口
│   ├── bsp_dma.c/h         #   DMA 配置与触发
│   ├── bsp_gpio.c/h        #   GPIO 读写（595 控制、DIO2/3 翻转）
│   ├── bsp_uart.c/h        #   UART4/USART1/USART10 接口
│   ├── bsp_qspi.c/h        #   OCTOSPI PSRAM 接口
│   └── bsp_clock.c/h       #   时钟树配置
│
├── Drivers/                # 外部器件驱动
│   ├── AD9959/             #   AD9959 DDS 芯片驱动
│   │   ├── ad9959.c/h      #     寄存器读写、Profile 控制、IO_UPDATE
│   │   └── ad9959_reg.h    #     寄存器地址与位定义
│   ├── 595/                #   74HC595 移位寄存器驱动
│   │   └── hc595.c/h       #     级联写入、位操作
│   ├── APS6404/            #   APS6404 PSRAM 驱动
│   │   └── aps6404.c/h     #     QSPI 读写、地址管理
│   ├── Clock/              #   时钟分发管理
│   │   └── clock_mgr.c/h   #     REF_CLK 频率配置
│   └── Trigger/            #   触发与同步管理
│       └── trigger.c/h     #     TIM2→DMA→TIM8 链路控制
│
├── Middleware/             # 核心发送流水线（与硬件解耦）
│   ├── DDS/                #   DDS 命令抽象
│   │   ├── dds_command.c/h #     DDS_Command 数据结构与队列
│   │   └── dds_encoder.c/h #     DDS_Command → SPI Frame 编码
│   ├── Buffer/             #   缓冲区管理
│   │   ├── ring_buffer.c/h #     环形缓冲区
│   │   ├── pingpong.c/h    #     乒乓缓冲（SRAM）
│   │   └── packet_buf.c/h  #     数据包缓冲（PSRAM）
│   ├── Scheduler/          #   发送调度器
│   │   └── scheduler.c/h   #     队列调度、优先级、定时发送
│   ├── Packet/             #   数据包组装
│   │   └── packet.c/h      #     Preamble/Sync/Header/Payload/CRC
│   ├── Symbol/             #   符号映射
│   │   └── symbol.c/h      #     Byte → Bit → Symbol
│   └── Modulator/          #   调制器（一种调制一个文件）
│       ├── mod_cw.c
│       ├── mod_fsk.c
│       ├── mod_gfsk.c
│       ├── mod_bpsk.c
│       ├── mod_qpsk.c
│       ├── mod_am.c
│       └── mod_fm.c
│
├── App/                    # 应用层业务逻辑
│   ├── tx_manager.c/h      #   发射管理
│   ├── cmd_parser.c/h      #   命令解析
│   └── shell.c/h           #   CLI 交互
│
├── Test/                   # 单元测试与集成测试
│
├── Docs/                   # 文档
│
└── Tools/                  # 上位机脚本与工具
```

---

# 4 Core

CubeMX 自动生成工程。

原则：

- 不修改用户代码区域之外内容
- 不放业务代码
- 不放算法

---

# 5 BSP（Board Support Package）

负责 MCU 硬件抽象。每个外设模块提供最小化接口，仅暴露本层需要的操作。

## 5.1 外设清单

| 外设 | 用途 | 关键引脚 |
|:--|:---|:---|
| SPI1 | AD9959 SDIO0 数据发送 (MOSI) + 读回 (MISO) + 片选 (NSS) + 时钟 (SCK) | PD7, PB4, PA15, PB3 |
| SPI3 | AD9959 SDIO1 数据发送 (TX Only) + 时钟 | PD6, PC10 |
| TIM2 | DMA 触发源 (CH1 PWM)，Update 经 ITR1 复位 TIM8 | PA0 |
| TIM4 | CS 时序测量 (CH1/CH2 Input Capture)，CNT 由 ETR 硬件清零 | PB6, PB7, PE0 |
| TIM8 | IO_UPDATE (CH1) + DIO3 (CH2) 脉冲输出，由 TIM2 Update → ITR1 复位 | PC6, PC7 |
| TIM15 | AD9959 REF_CLK 参考时钟 (CH1 PWM) | PC12 |
| DMA | SPI1_TX + SPI3_TX 并行搬运，由 TIM2_CH1 触发 | — |
| GPIO | DIO2 预留 (PD5)、595 控制 (PE2~PE6)、通用 IO (PD0~PD3) | PD5, PE2~PE6, PD0~PD3 |
| UART4 | AD9959 辅助通信 IO | PD0, PD1 |
| USART1 | 调试串口 | PA9, PA10 |
| USART10 | 595 串行数据 (半双工单线) | PE3 |
| FDCAN1 | CAN 总线通信 | PA11, PA12 |
| OCTOSPI1 | PSRAM (APS6404) | PB2, PB10, PB13, PD11, PD12, PD13 |
| OPAMP1/2 | 模拟前端 | PB0, PC4, PE9, PE7 |

## 5.2 接口示例

```c
/* SPI */
void bsp_spi_transmit(SPI_TypeDef *spi, const uint8_t *data, uint16_t size);
void bsp_spi_dma_start(SPI_TypeDef *spi, const uint8_t *buf, uint16_t size);

/* Timer */
void bsp_tim_start(TIM_TypeDef *tim);
void bsp_tim_stop(TIM_TypeDef *tim);
void bsp_tim_set_ccr(TIM_TypeDef *tim, uint8_t channel, uint32_t value);
uint32_t bsp_tim_get_capture(TIM_TypeDef *tim, uint8_t channel);

/* GPIO */
void bsp_gpio_set(GPIO_TypeDef *port, uint16_t pin);
void bsp_gpio_clear(GPIO_TypeDef *port, uint16_t pin);
```

## 5.3 禁止事项

BSP 不允许出现业务相关符号：

- `DDS`
- `FSK` / `PSK` / `GFSK`
- `AD9959`
- `Profile`

---

# 6 Drivers

驱动具体器件，依赖 BSP 层接口。

## 6.1 AD9959

负责 DDS 芯片的底层操作。

**功能：**

- SPI 寄存器读写（通过 SPI1 + SPI3 并行 + GPIO 辅助）
- Profile 寄存器选择
- IO_UPDATE 脉冲控制
- Reset / PowerDown 控制（经 595 间接操作）

**接口：**

```c
void     AD9959_Init(void);
void     AD9959_WriteRegister(uint8_t addr, uint32_t data);
uint32_t AD9959_ReadRegister(uint8_t addr);
void     AD9959_IOUpdate(void);
void     AD9959_SelectProfile(uint8_t profile);
```

**关键设计点：**

AD9959 采用**双线串行模式 (2-wire)**：

- SDIO0 (SPI1_MOSI, PD7) — 数据线 0
- SDIO1 (SPI3_MOSI, PD6) — 数据线 1
- SDIO2 (PD5, GPIO) — **预留**，当前未使用
- SCLK — SPI1_SCK (PB3) 与 SPI3_SCK (PC10) 同步同频

TIM2_CH1 (PA0) 作为 DMA 时钟源，每个上升沿同时触发两路 DMA 从乒乓缓冲区搬运数据到 SPI1 和 SPI3 的 TX FIFO，SPI 随即开始发送。

驱动层仅提供寄存器访问能力，不实现任何调制算法。

## 6.2 74HC595（OCR 控制）

负责两片级联 595 移位寄存器的控制，提供板级 IO 扩展。

**硬件连接：**

- PE2 → STCP (RCLK)：锁存脉冲
- PE3 (USART10_TX) → DS (SER)：串行数据
- PE4 → SHCP (SRCLK)：移位时钟
- PE5 → MR (SRCLR)：主复位
- PE6 → OE：输出使能

**位分配：**

| 芯片 | 位 | 信号 | 用途 |
|:--|:--|:---|:---|
| #1 | 0 | NC | 未连接 |
| #1 | 1 | LEDSTBY | 待机指示灯 |
| #1 | 2 | LED ANALOGREADY | 模拟前端就绪指示灯 |
| #1 | 3 | LEDMODREADY | 调制/模式就绪指示灯 |
| #1 | 4 | LEDCH0TRANSMIT | CH0 发射指示 |
| #1 | 5 | LEDCH1TRANSMIT | CH1 发射指示 |
| #1 | 6 | LEDCH2TRANSMIT | CH2 发射指示 |
| #1 | 7 | LEDCH3TRANSMIT | CH3 发射指示 |
| #2 | 0 | NC | 未连接 |
| #2 | 1 | DDSPWREN1V8D | DDS 数字 1.8V 电源使能 |
| #2 | 2 | DDSPWREN1V8A | DDS 模拟 1.8V 电源使能 |
| #2 | 3 | DDSMASTERRST | DDS 主复位 |
| #2 | 4 | DDSPDN | DDS 掉电控制 |
| #2 | 5 | DDSPWREN_3V3D | DDS 数字 3.3V 电源使能 |
| #2 | 6 | DDSCLKMODE33 | REF_CLK 模式选择 |
| #2 | 7 | NC | 未连接 |

**接口：**

```c
void HC595_Init(void);
void HC595_Write(uint16_t data);       /* 写入 16 位（两片） */
void HC595_SetBit(uint8_t chip, uint8_t bit, bool value);
void HC595_Latch(void);                /* STCP 脉冲 */
void HC595_Reset(void);                /* MR 脉冲 */
void HC595_Enable(void);               /* OE 拉低 */
void HC595_Disable(void);              /* OE 拉高 */
```

## 6.3 APS6404

负责 PSRAM 初始化、数据读写、地址管理，提供统一存储接口。

## 6.4 Clock

负责时钟分发管理：

- REF_CLK 频率配置（TIM15 CH1 PWM 输出到 AD9959）
- 系统时钟树验证

## 6.5 Trigger

负责 TIM2 → DMA → SPI → TIM8 硬件触发链路的配置与控制。详见第 7 节。

---

# 7 硬件接口与 DMA 流水线

## 7.1 整体硬件架构

```
                        ┌── DMA_REQ ──► SPI1_TX (PD7) ──► AD9959 SDIO0
                        │    DMA_REQ ──► SPI3_TX (PD6) ──► AD9959 SDIO1
                        │
PA0 (lptim3_out) ─────────┤
(SPI_9959_DMA_TRIG)     ├── 外部飞线 ──► PE0 (TIM4_ETR)
                        │                 上升沿 → ETR 硬件自动清零 TIM4 CNT
                        │
                        └── LPTIM3_OUT ──► ITR1 ──► TIM8 Reset
                                                      │
                                                      ├── TIM8_CH1 (PC6) → IO_UPDATE 脉冲
                                                      └── TIM8_CH2 (PC7) → DIO3 脉冲

PA15 (SPI1_NSS/CS) ─────┬── 直连 ──► PB6 (TIM4_CH1)  CS↓ 捕获 TIM4 计数
                         └── 直连 ──► PB7 (TIM4_CH2)  CS↑ 捕获 TIM4 计数

PC12 (TIM15_CH1) ────────────────► AD9959 REF_CLK (参考时钟)
```

## 7.2 双线模式数据传输

AD9959 工作于**双线串行模式**，仅使用 SDIO0 和 SDIO1 两条数据线：

| AD9959 引脚 | STM32 信号 | 实现方式 |
|:--|:--|:--|
| SDIO0 | SPI1_MOSI (PD7) | SPI1 硬件 MOSI |
| SDIO1 | SPI3_MOSI (PD6) | SPI3 硬件 MOSI |
| SDIO2 | GPIO PD5 | **预留**，当前未使用 |
| SCLK | SPI1_SCK (PB3) | SPI1 时钟（SPI3_SCK (PC10) 同步同频） |
| CS | SPI1_NSS (PA15) | 硬件 NSS |

TIM2_CH1 (PA0) 作为时钟源，每个上升沿同时触发两路 DMA 从乒乓缓冲区搬运数据到 SPI1 和 SPI3 的 TX FIFO，SPI 随即开始并行发送。

## 7.3 时序流程

```
    TIM2_CH1 (PA0)
    ──┐         ┌──────────────────────────
      └─────────┘  上升沿
      │         │
      │         ├─ (1) DMA 请求 → SPI1 + SPI3 同时从乒乓缓冲搬运数据到 TX FIFO
      │         ├─ (2) TIM4_ETR (PE0) 检测上升沿 → 硬件自动清零 TIM4 CNT
      │         └─ (3) TIM2 Update (TRGO) → ITR1 → TIM8 计数器复位 (Reset Mode)
      │
    SPI_CS (PA15, 硬件 NSS)
      │  ┌─────────────────┐
      └──┘    SPI 通信      └────────────────────
         │                 │
         ├─ CS↓ → TIM4_CH1 (PB6) 捕获 → t1 = DMA 触发到 SPI 开始通信
         └─ CS↑ → TIM4_CH2 (PB7) 捕获 → t2 = DMA 触发到 SPI 通信结束

    SPI1_SCK (PB3) + SPI3_SCK (PC10)  并行时钟，125 Mbps
    SPI1_MOSI (PD7) + SPI3_MOSI (PD6) 同步发送数据

    TIM8_CH1 (PC6) + TIM8_CH2 (PC7)
      │                              ┌─┐
      └──────────────────────────────┘ └──────────
                                     ↑
                   校准后的比较值到达 → CH1 输出 IO_UPDATE 脉冲
                                      → CH2 输出 DIO3 脉冲
                                     刷新 AD9959 IO 寄存器
```

## 7.4 TIM4 时序测量（调试用）

| 参数 | 来源 | 用途 |
|:--|:---|:---|
| t_DMA_to_CS↓ | TIM4_CH1 (PB6) 捕获值 | DMA 触发到 SPI 开始通信的延时 |
| t_DMA_to_CS↑ | TIM4_CH2 (PB7) 捕获值 | DMA 触发到 SPI 通信结束的延时 |
| t_CS_active | CH2 - CH1 | SPI 通信持续时间 |

- TIM4 CNT 由 ETR (PE0，直连 PA0) 在 TIM2_CH1 上升沿**硬件自动清零**
- 捕获值用于**调试阶段**评估 DMA→SPI 的实际延时，帮助开发者选择合适的 TIM8 CH1/CH2 比较值

## 7.5 TIM8 同步脉冲

TIM8 在从模式 (Reset Mode) 下由 TIM2 Update 事件通过 ITR1 复位：

- **TIM8_CH1 (PC6)**：比较匹配后输出 IO_UPDATE 脉冲给 AD9959
- **TIM8_CH2 (PC7)**：比较匹配后输出脉冲给 AD9959 DIO3

CH1 和 CH2 的比较值在校准阶段根据 TIM4 实测延时确定，确保脉冲落在 SPI 通信完成之后，可靠地刷新 AD9959 IO 寄存器。

## 7.6 乒乓缓冲机制

```
    PSRAM (大容量)           SRAM Ping-Pong          DMA Buffer (SPI)
    ┌──────────┐           ┌──────────┐           ┌──────────┐
    │ Packet N │           │  Bank A  │           │  SPI1 TX │──► PD7 (SDIO0)
    │ Packet   │──CPU──►   │  ready   │──DMA──►   │  SPI3 TX │──► PD6 (SDIO1)
    │ N+1      │  填充     ├──────────┤  搬运     └──────────┘
    │ Packet   │           │  Bank B  │
    │ N+2      │           │  filling │
    └──────────┘           └──────────┘
```

- CPU 后台从 PSRAM 填充空闲 Bank
- DMA 仅在 SRAM 与 SPI 之间搬运（DMA 不直接访问 PSRAM）
- 两个 Bank 交替使用，保证连续发送无间断

---

# 8 Middleware（概述）

Middleware 负责整个发送流程，是工程的核心。

包括模块：

- **DDS** — DDS 命令抽象与编码
- **Buffer** — 缓冲区管理
- **Scheduler** — 发送调度
- **Packet** — 数据包组装
- **Symbol** — 符号映射
- **Modulator** — 调制算法
- **Pulse Shaping** — 数字脉冲整形

---

# 9 DDS 模块

DDS 模块负责管理 DDS 数据，向上层屏蔽具体 DDS 型号的差异。

```
DDS_Command
    ↓
Encoder
    ↓
SPI Frame
    ↓
DMA
```

DDS 模块不知道调制方式（FSK / ASK / QPSK 等），仅处理 DDS 命令。

**统一数据结构：**

```c
typedef struct
{
    uint32_t ftw;        /* Frequency Tuning Word (32-bit) */
    uint16_t asf;        /* Amplitude Scale Factor (10-bit effective) */
    uint16_t pow;        /* Phase Offset Word (14-bit effective) */
    uint8_t  profile;    /* 目标 Profile (0–7) */
} DDS_Command;
```

以后更换 DDS（如 AD9910、AD9854、FPGA DDS），仅修改 Encoder 即可。

---

# 10 Buffer 模块

负责所有缓冲区管理。

包括：

- Ring Buffer — 通用环形缓冲
- Frame Buffer — SPI 帧缓冲
- Ping Pong Buffer — 双缓冲 DMA 数据
- Packet Buffer — PSRAM 数据包缓冲

**数据流：**

```
PSRAM (Packet Buffer)
    ↓ CPU 填充
SRAM Ping-Pong
    ↓ DMA 搬运
DMA Buffer (SPI TX)
```

DMA 仅访问 SRAM，CPU 后台从 PSRAM 填充。

---

# 11 Scheduler 模块

负责所有发送调度。

```
Packet Queue
    ↓
DDS Queue
    ↓
DMA Queue
```

未来支持：

- 连续发送
- 定时发送
- Beacon
- 跳频
- 多任务

Scheduler 统一管理。

---

# 12 Packet 模块

Packet 负责生成完整的数据包。

包括：Preamble、Sync、Header、Payload、CRC、FEC、Scramble。

以后可支持：AX.25、自定义协议、串口透传、无线升级。

---

# 13 Symbol 模块

Packet 转换为 Symbol。

```
Byte → Bit → Symbol
```

适用于 BPSK、QPSK、FSK、MFSK 等。Symbol 层不关心 DDS。

---

# 14 Pulse Shaping（脉冲整形）

负责数字滤波。

可选滤波器：Gaussian、Raised Cosine、Root Raised Cosine、Half Sine。

示例：

- GFSK：Bit → Gaussian FIR → Frequency Sequence
- MSK：Half Sine
- QPSK：RRC

---

# 15 Modulator

调制器负责将 Symbol 转换为 DDS_Command。

```
Modulator/
    mod_cw.c
    mod_fsk.c
    mod_gfsk.c
    mod_bpsk.c
    mod_qpsk.c
    mod_am.c
    mod_fm.c
```

**统一接口：**

```c
void Modulator_Process(const Symbol *sym, DDS_Command *cmd);
void Modulator_Init(const ModConfig *cfg);
```

输出：`DDS_Command[]`

---

# 16 DDS Encoder

负责将 `DDS_Command` 转换为实际 SPI 帧。

```
DDS_Command → AD9959 寄存器地址 + 数据 → 4 线并行 SPI Frame (32-bit)
```

更换 DDS（如 AD9910、AD9854、FPGA DDS）时，仅修改 Encoder 即可。

---

# 17 电源与初始化序列

## 17.1 DDS 上电顺序

1. 595 全部输出初始化（OE 禁止输出）
2. `DDSPWREN1V8D` 使能（数字 1.8V）
3. `DDSPWREN1V8A` 使能（模拟 1.8V）
4. 等待电源稳定
5. `DDSPWREN_3V3D` 使能（数字 3.3V）
6. `DDSCLKMODE33` 配置 REF_CLK 模式
7. `DDSMASTERRST` 脉冲（释放复位）
8. 595 OE 使能，595 数据锁存
9. AD9959 初始化（SPI 寄存器配置）
10. REF_CLK (TIM15) 启动

## 17.2 发送启动序列

1. Modulator 填充 DDS_Command 到缓冲区
2. Encoder 把 DDS_Command 转码为 SPI 帧写入 Ping-Pong Buffer
3. TIM2 启动，首次 CH1 上升沿同时触发两路 DMA 搬运数据到 SPI1 + SPI3
4. SPI1 与 SPI3 随即并行发送（双线模式）、CS 硬件拉低
5. TIM4 ETR 检测 CH1 上升沿，硬件清零 CNT；CH1/CH2 分别在 CS↓/CS↑ 捕获
6. 传输结束，CS 硬件拉高
7. TIM8 被 TIM2 Update 复位，CH1/CH2 在各自校准延迟后输出脉冲 → IO_UPDATE + DIO3
8. AD9959 IO 寄存器刷新，新的频率/相位/幅度生效
9. 循环回到步骤 3（Ping-Pong 切换）

---

# 18 数据流

整个系统的数据流固定如下：

```
Payload
      │
      ▼
 Packet
      │
      ▼
 Symbol
      │
      ▼
 Pulse Shaping
      │
      ▼
 Modulator
      │
      ▼
 DDS_Command
      │
      ▼
 Encoder
      │
      ▼
 DMA Buffer
      │
      ▼
SPI DMA
      │
      ▼
 AD9959
```

任何新的调制方式均应遵循上述流程。

---

# 19 模块依赖关系

```
App
 │
 ▼
Packet
 │
 ▼
Symbol
 │
 ▼
Pulse Shape
 │
 ▼
Modulator
 │
 ▼
DDS
 │
 ▼
Driver
 │
 ▼
BSP
```

**允许调用方向：**

```
上层 → 下层
```

**禁止：**

```
Driver ← App     (跨层反向调用)
```

---

# 20 后续开发路线

| 阶段 | 内容 | 验证目标 |
|:--|:---|:---|
| Phase 1 | BSP、AD9959 驱动、DMA Pipeline | SPI 可自动发送并完成 IO_UPDATE |
| Phase 2 | DDS_Command、Encoder、乒乓缓冲 | 可连续输出固定频率、幅度和相位 |
| Phase 3 | Scheduler、Packet、Buffer | 支持异步发送和队列管理 |
| Phase 4 | FSK、ASK、CW | 完成基础调制验证 |
| Phase 5 | GFSK、MSK | 加入高斯滤波和连续相位调制 |
| Phase 6 | BPSK、QPSK、QAM | 完善符号映射和相位控制 |
| Phase 7 | FHSS、脚本控制、录波回放 | 构建完整的软件定义发射机平台 |

---

# 21 设计原则

1. **平台无关**：调制算法与 STM32 外设、AD9959 驱动解耦。
2. **层次清晰**：每层只负责一种职责，通过公开 API 通信，禁止跨层访问。
3. **异步流水线**：CPU 负责生成数据，DMA 和定时器负责发送，实现后台连续工作。
4. **可扩展性**：新增调制方式应尽量只增加新的 `mod_xxx.c` 或 `pulse_shape.c` 模块，不修改已有框架。
5. **可移植性**：将来若更换为 AD9910、FPGA DDS 或高速 DAC，仅需替换 Encoder 与 Pipeline 层，上层协议、调制、调度和数据处理保持不变。
