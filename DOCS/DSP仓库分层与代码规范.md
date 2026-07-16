# DSP仓库分层与代码规范

## 1. 仓库分层

推荐仓库结构如下：

```text
Repo/
├─ README.md
├─ docs/
├─ modules/
│  ├─ bsp/
│  ├─ drivers/
│  ├─ services/
│  ├─ app/
│  ├─ ui/
│  └─ debug/
├─ projects/
├─ templates/
└─ tools/
```

### 1.1 `bsp`

板级支持包，负责 MCU 片上基础外设。

包含内容：

- `gpio`
- `uart`
- `spi`
- `i2c`
- `dma`
- `tim`
- `pwm`
- `adc`
- `exti`

要求：

- 只提供底层硬件访问能力
- 不写业务逻辑
- 不写具体项目流程

### 1.2 `drivers`

具体器件驱动层，负责板外芯片和模块封装。

包含内容：

- 传感器驱动
- 显示驱动
- DDS/PLL 驱动
- ADC/DAC 驱动
- 电机驱动
- 编码器驱动

要求：

- 只关心器件本身如何初始化、配置、读写
- 不夹杂菜单、任务流程、比赛逻辑

### 1.3 `services`

通用服务层，负责在 `bsp` 和 `drivers` 之上做功能组合。

包含内容：

- 滤波
- PID
- 按键服务
- 菜单服务
- 软件定时器
- 协议解析
- 采样调度

要求：

- 提供可复用的功能模块
- 不直接操作寄存器
- 不绑定具体项目业务

### 1.4 `app`

应用层，负责整机业务逻辑。

包含内容：

- 状态机
- 模式切换
- 比赛流程
- 多模块协同控制

要求：

- 只调用 `services`、`drivers`、`ui`
- 不直接深入底层硬件细节

### 1.5 `ui`

显示与交互层。

包含内容：

- OLED 页面
- LCD 页面
- LVGL 页面
- 参数调节页面
- 波形显示页面

要求：

- 负责显示组织和交互
- 不直接操作底层寄存器

### 1.6 `debug`

调试辅助层。

包含内容：

- 日志输出
- 断言
- 运行状态监视
- 调试命令

要求：

- 提供统一调试入口
- 不污染主业务逻辑

## 2. 分层规则

### 2.1 黄金法则

- `drivers` 和 `app` 必须严格分开
- `ui` 只负责显示和交互，不负责底层驱动
- `bsp` 只负责底层访问，不负责业务
- `services` 负责沉淀可复用功能，不直接写项目流程

### 2.2 允许依赖关系

允许的依赖方向如下：

```text
app -> services / drivers / ui
ui  -> services / drivers
services -> drivers / bsp
drivers -> bsp
bsp -> 芯片SDK/HAL
debug -> 各层按需调用
```

禁止：

- `bsp -> app`
- `drivers -> app`
- `drivers -> ui`
- `services -> app`

### 2.3 模块目录规范

每个模块建议使用统一结构：

```text
module_name/
├─ inc/
│  └─ module_name.h
├─ src/
│  └─ module_name.c
├─ example/
│  └─ example_basic.c
├─ test/
│  └─ test_module_name.c
└─ README.md
```

## 3. 代码规范

### 3.1 文件命名

- 文件名应清晰表达模块功能
- 推荐小写加下划线，或与模块名保持一致
- 禁止使用无意义命名

推荐：

- `ad9959.c`
- `ad9959.h`
- `menu_service.c`
- `soft_timer.c`

禁止：

- `new.c`
- `abc.c`
- `test1.c`

### 3.2 函数命名

统一格式：

`模块名_动作`

示例：

- `AD9959_Init()`
- `AD9959_SetFreq()`
- `OLED_ShowString()`
- `Menu_ProcessKey()`
- `PID_Calc()`

### 3.3 类型命名

结构体：

`模块名_TypeDef`

示例：

- `PID_TypeDef`
- `Menu_Item_TypeDef`
- `AD9959_TypeDef`

枚举：

`模块名_类别`

示例：

- `Module_Status`
- `DDS_WaveType`
- `Key_Event`

宏：

全大写加下划线。

示例：

- `PID_OUTPUT_MAX`
- `OLED_PAGE_COUNT`
- `KEY_SCAN_PERIOD_MS`

### 3.4 变量命名

- 局部变量使用小写加下划线
- 全局变量尽量少
- 文件内私有全局变量必须加 `static`
- 与单位相关的变量应体现单位

示例：

- `sample_count`
- `freq_hz`
- `voltage_mv`
- `phase_deg`

### 3.5 函数设计

- 一个函数只做一件事
- 函数长度尽量可控
- 超过 50 行优先考虑拆分
- 嵌套层数尽量不超过 3 层
- 接口职责明确

不允许：

- 一个函数同时做初始化、采样、显示、发送串口

### 3.6 参数与返回值

- 只读参数尽量加 `const`
- 需要修改对象状态时传结构体指针
- 多返回值使用输出指针或结果结构体
- 所有输入指针都必须判空

推荐统一状态码：

```c
typedef enum
{
    MODULE_OK = 0,
    MODULE_ERROR = -1,
    MODULE_BUSY = -2,
    MODULE_TIMEOUT = -3,
    MODULE_INVALID_PARAM = -4
} Module_Status;
```

### 3.7 对外接口规范

#### 3.7.1 最小暴露原则

- 对外只暴露“必须给别人用”的接口
- 模块内部辅助函数必须放在 `.c` 文件内并加 `static`
- 不允许通过头文件暴露内部临时变量、调试变量、过程函数

推荐：

- 对外暴露 `Init`、`Config`、`Set`、`Get`、`Process`、`Reset`

不推荐：

- 暴露只在本模块内部使用的 `xxx_delay_us()`、`xxx_write_bit()`、`xxx_temp_calc()`

#### 3.7.2 头文件收口原则

- 每个模块原则上只有一个主对外头文件
- 对外只 `#include` 必要依赖
- 不要在公共头文件里包含大批无关头文件
- 不要把私有宏、私有结构体、私有静态缓存暴露到 `inc/`

推荐：

- `inc/ad9959.h` 作为公共入口
- 私有定义放在 `src/ad9959.c` 或模块私有头文件中

#### 3.7.3 接口分层原则

对外接口应按层次拆分：

- 基础初始化接口
- 参数配置接口
- 数据读写接口
- 运行处理接口
- 状态查询接口
- 复位接口

推荐示例：

```c
Module_Status AD9959_Init(AD9959_TypeDef *dev);
Module_Status AD9959_SetFreq(AD9959_TypeDef *dev, uint8_t ch, uint32_t freq_hz);
Module_Status AD9959_SetPhase(AD9959_TypeDef *dev, uint8_t ch, float phase_deg);
Module_Status AD9959_EnableOutput(AD9959_TypeDef *dev, uint8_t ch, uint8_t enable);
```

#### 3.7.4 接口命名一致性

- 同类模块应尽量使用相同动作词
- 不要同一仓库里同时出现 `Init` / `Open` / `Create` 混用而含义相近
- 不要同一仓库里同时出现 `GetValue` / `ReadValue` / `FetchValue` 混用

建议统一动作词：

- `Init`
- `DeInit`
- `Config`
- `Set`
- `Get`
- `Read`
- `Write`
- `Start`
- `Stop`
- `Process`
- `Reset`

#### 3.7.5 接口参数规范

- 参数顺序保持稳定，优先“对象 -> 通道/索引 -> 输入参数 -> 输出参数”
- 单位必须在参数名中体现
- 布尔意义参数尽量限制取值范围，不直接使用含义不明的裸整数
- 输出参数放在最后

推荐：

```c
Module_Status ADC_ReadChannel(ADC_DeviceTypeDef *dev, uint8_t channel, uint16_t *value);
Module_Status DDS_SetFreq(AD9959_TypeDef *dev, uint8_t channel, uint32_t freq_hz);
```

不推荐：

```c
int set(uint32_t a, int b, int c);
```

#### 3.7.6 返回值规范

- 对外接口除纯计算函数外，优先返回统一状态码
- 不要返回含义不清的 `0/1`
- 返回值表示执行结果，真实数据尽量走输出参数

推荐：

```c
Module_Status ADS1115_ReadVoltage(ADS1115_TypeDef *dev, uint8_t channel, float *voltage_mv);
```

#### 3.7.7 结构体暴露规范

- 对外结构体只保留必要字段
- 硬件寄存器缓存、内部状态机细节、临时计算变量尽量不暴露
- 若模块内部状态复杂，可只前置声明句柄并隐藏实现

推荐：

- 对简单模块直接暴露 `TypeDef`
- 对复杂模块使用“句柄 + 接口函数”风格

#### 3.7.8 宏与配置项规范

- 对外宏只保留用户必须配置或必须引用的内容
- 可改参数优先使用配置结构体，不要大量依赖改宏
- 宏名必须带模块前缀

推荐：

```c
#define OLED_WIDTH       128U
#define OLED_HEIGHT      64U
#define KEY_EVENT_MAX    8U
```

#### 3.7.9 回调接口规范

- 只有在确实需要异步通知时才提供回调
- 回调函数命名、参数、调用时机必须写清
- 中断上下文触发的回调必须明确说明

推荐：

```c
typedef void (*Uart_RxCallback)(uint8_t *data, uint16_t len);
```

要求：

- 文档中写明回调在哪个上下文执行
- 写明是否允许阻塞
- 写明是否允许调用其他模块接口

#### 3.7.10 中断安全与线程安全说明

每个对外接口如果存在上下文限制，必须注明：

- 是否可在中断中调用
- 是否可在任务中调用
- 是否可重入
- 是否依赖外部锁

推荐写法：

```c
/**
 * @brief  Push one byte into fifo
 * @param  fifo Pointer to fifo object
 * @param  data Input byte
 * @return MODULE_OK for success, otherwise error code
 * @note   This function can be called in ISR context.
 */
```

#### 3.7.11 兼容性规范

- 已公开使用的接口，不应随意修改函数名、参数顺序、返回值语义
- 若必须变更，应同步更新文档和示例
- 对公共仓库中的稳定模块，优先“新增接口”而不是“直接改旧接口”

#### 3.7.12 示例要求

- 每个对外模块必须至少有一个最小调用示例
- 示例必须覆盖初始化和核心接口
- 示例代码应只展示公开接口，不直接调用私有实现

### 3.8 宏与常量

- 编译期开关、寄存器位、固定常量可使用宏
- 运行期只读常量优先使用 `const`
- 魔法数必须消除

推荐：

```c
#define KEY_SCAN_PERIOD_MS 10U
static const float wheel_radius_m = 0.0325f;
```

### 3.8 注释规范

注释应解释：

- 为什么这样做
- 时序或公式依据
- 硬件限制
- 边界条件

禁止废话注释。

不推荐：

```c
count++; /* count加1 */
```

推荐：

```c
/* 回波脉宽单位为us，乘0.017后换算为cm */
distance_cm = echo_us * 0.017f;
```

### 3.10 中断规范

中断中只允许：

- 置标志位
- 简单计数
- 极短数据搬运

中断中禁止：

- 长时间阻塞
- 大量浮点计算
- 刷屏
- 长串口打印

### 3.11 排版规范

- 统一 4 空格缩进
- `if`、`for`、`while` 后保留空格
- 运算符两侧保留空格
- 左右大括号风格全仓统一

推荐风格：

```c
if (distance_cm < 5.0f)
{
    stop_flag = 1;
}
```

## 4. 文件头规范

每个 `.c/.h` 文件建议统一文件头，至少包含：

- 文件名
- 模块功能
- 作者或维护者
- 创建日期
- 修改说明
- 版权或归属说明

推荐模板：

```c
/**
 * @file    pid.h
 * @brief   PID control module interface
 * @author  xxx
 * @date    2026-07-09
 * @version 1.0
 * @note    Provide position PID basic interfaces.
 */
```

要求：

- 文件头内容简洁
- `@brief` 一句话说明文件职责
- 英文标签统一，不混写

## 5. Doxygen 注释规范

所有对外接口函数、结构体、枚举、宏定义建议使用 Doxygen 风格注释。

### 5.1 函数注释模板

```c
/**
 * @brief  Initialize PID object
 * @param  pid Pointer to PID object
 * @param  kp Proportional coefficient
 * @param  ki Integral coefficient
 * @param  kd Derivative coefficient
 * @return None
 */
void PID_Init(PID_TypeDef *pid, float kp, float ki, float kd);
```

要求：

- `@brief` 简明描述函数作用
- `@param` 每个参数都要写
- `@return` 必须写返回含义
- 若有特殊限制，用 `@note`

### 5.2 结构体注释模板

```c
/**
 * @brief PID controller object
 */
typedef struct
{
    float kp;              /**< Proportional coefficient */
    float ki;              /**< Integral coefficient */
    float kd;              /**< Derivative coefficient */
    float target;          /**< Target value */
    float integral;        /**< Integral accumulator */
    float output_limit;    /**< Output limit */
} PID_TypeDef;
```

### 5.3 枚举注释模板

```c
/**
 * @brief Common module status
 */
typedef enum
{
    MODULE_OK = 0,             /**< Operation success */
    MODULE_ERROR = -1,         /**< Generic error */
    MODULE_BUSY = -2,          /**< Device or module busy */
    MODULE_TIMEOUT = -3,       /**< Timeout occurred */
    MODULE_INVALID_PARAM = -4  /**< Invalid input parameter */
} Module_Status;
```

### 5.4 宏注释规范

关键宏定义建议使用行尾注释说明含义。

```c
#define PID_OUTPUT_MAX      1000.0f  /**< PID output upper limit */
#define KEY_SCAN_PERIOD_MS  10U      /**< Key scan period in ms */
```

## 6. 代码示例

下面给出一个符合本规范的头文件示例。

```c
/**
 * @file    pid.h
 * @brief   PID control module interface
 * @author  EC Club
 * @date    2026-07-09
 * @version 1.0
 * @note    Provide position PID interfaces.
 */

#ifndef PID_H
#define PID_H

#include <stdint.h>

/**
 * @brief Common module status
 */
typedef enum
{
    MODULE_OK = 0,             /**< Operation success */
    MODULE_ERROR = -1,         /**< Generic error */
    MODULE_BUSY = -2,          /**< Module busy */
    MODULE_TIMEOUT = -3,       /**< Timeout occurred */
    MODULE_INVALID_PARAM = -4  /**< Invalid parameter */
} Module_Status;

/**
 * @brief PID controller object
 */
typedef struct
{
    float kp;              /**< Proportional coefficient */
    float ki;              /**< Integral coefficient */
    float kd;              /**< Derivative coefficient */
    float target;          /**< Target value */
    float measure;         /**< Measured value */
    float error;           /**< Current error */
    float last_error;      /**< Last cycle error */
    float integral;        /**< Integral accumulator */
    float output;          /**< Controller output */
    float output_limit;    /**< Output limit */
    float integral_limit;  /**< Integral limit */
} PID_TypeDef;

/**
 * @brief  Initialize PID object
 * @param  pid Pointer to PID object
 * @param  kp Proportional coefficient
 * @param  ki Integral coefficient
 * @param  kd Derivative coefficient
 * @return MODULE_OK for success, otherwise error code
 * @note   Caller must provide a valid PID object pointer.
 */
Module_Status PID_Init(PID_TypeDef *pid, float kp, float ki, float kd);

/**
 * @brief  Set PID target value
 * @param  pid Pointer to PID object
 * @param  target Target value
 * @return MODULE_OK for success, otherwise error code
 */
Module_Status PID_SetTarget(PID_TypeDef *pid, float target);

/**
 * @brief  Calculate PID output
 * @param  pid Pointer to PID object
 * @param  measure Current measured value
 * @return Calculated controller output
 */
float PID_Calc(PID_TypeDef *pid, float measure);

/**
 * @brief  Reset PID internal state
 * @param  pid Pointer to PID object
 * @return MODULE_OK for success, otherwise error code
 */
Module_Status PID_Reset(PID_TypeDef *pid);

#endif
```

## 7. 最低执行要求

提交到公共仓库的模块，至少必须满足：

1. 分层位置正确
2. `inc/` 与 `src/` 分离
3. 命名符合规范
4. 文件头完整
5. 对外接口具备 Doxygen 注释
6. 无明显业务逻辑污染驱动层
