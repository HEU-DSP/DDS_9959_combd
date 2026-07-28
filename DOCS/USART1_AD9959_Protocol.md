# DDS-9959 串口协议（固件 V1.0）

本文档对应当前工程中的 `App/host_protocol.c` 实现，供上位机和联调使用。

## 1. 物理接口

| 项目 | 规定 |
|---|---|
| 外设 | USART1 |
| MCU 引脚 | PA9 = TX，PA10 = RX |
| 串口格式 | 115200 baud，8 data bits，no parity，1 stop bit（8N1） |
| 连线 | USB-TTL 的 TX 接 PA10；USB-TTL 的 RX 接 PA9；必须共地 |
| 流控 | 无 |

MCU 采用 DMA 环形接收；上位机可以连续发送帧。建议发送一帧后等待对应 `SEQ` 的应答，再发送下一帧，便于故障定位。

## 2. 通用帧格式

所有多字节整数均为 **小端序（Little Endian）**。

| 偏移 | 字段 | 长度 | 固定值/含义 |
|---:|---|---:|---|
| 0 | SOF1 | 1 | `A5` |
| 1 | SOF2 | 1 | `5A` |
| 2 | VER | 1 | `01` |
| 3 | SEQ | 1 | 上位机自定义序号；应答原样回传 |
| 4 | CMD | 1 | 命令号 |
| 5 | LEN_L | 1 | Payload 长度低字节 |
| 6 | LEN_H | 1 | Payload 长度高字节 |
| 7 | PAYLOAD | LEN | 命令参数或应答数据 |
| 7+LEN | CRC_L | 1 | CRC16 低字节 |
| 8+LEN | CRC_H | 1 | CRC16 高字节 |

- 最短帧：9 字节。
- 最大 Payload：64 字节；最大总帧：73 字节。
- CRC：CRC-16/CCITT-FALSE，`poly=0x1021`，`init=0xFFFF`，无反射，`xorout=0x0000`。
- CRC 覆盖范围：从 `VER` 到 Payload 最后一个字节，不包含 `A5 5A` 和 CRC 本身。
- CRC 不正确的帧会被静默丢弃；不会回 NACK。

PING 示例：

```text
请求: A5 5A 01 00 00 00 00 5D BB
应答: A5 5A 01 00 00 04 00 01 01 00 00 74 B4
```

其中 PING 应答 Payload 为 `协议版本=1，固件主版本=1，次版本=0，保留=0`。

## 3. 公共字段与约束

| 名称 | 类型 | 约束 |
|---|---|---|
| `ch` | `u8` | 通道号 `0..3`，分别对应 AD9959 CH0..CH3 |
| `freq_hz` | `u32 LE` | 单位 Hz；固件换算 FTW。建议限制在 AD9959 实际可用输出频段内 |
| `asf` | `u16 LE` | 幅度比例因子，范围 `0..1023`；`1023` 为最大幅度 |
| `phase` | `u16 LE` | AD9959 原始 14-bit CPOW，范围 `0..0x3FFF`；`0x4000` 对应一整圈 |

频率由固件按当前 SYSCLK `490,909,091 Hz` 换算：

```text
FTW = round(freq_hz × 2^32 / 490909091)
```

## 4. 命令列表

| CMD | 名称 | Payload | 应答 |
|---:|---|---|---|
| `00` | PING | 无 | 固件/协议版本（4 B） |
| `01` | GET_STATUS | 无 | 4 个通道各 3 B，共 12 B |
| `02` | SET_CW | 7 B | ACK |
| `03` | SET_FSK | 11 B | ACK |
| `04` | SET_ASK | 9 B | ACK |
| `05` | SET_BPSK | 11 B | ACK |
| `06` | SET_QPSK | 7 B | ACK |
| `07` | SET_4FSK | 19 B | ACK |
| `08` | SET_SYMBOLS | 2+N B | ACK |
| `09` | APPLY | 无 | ACK + 有效通道掩码（1 B） |
| `0A` | STOP_OUTPUT | 无 | ACK |
| `0B` | GET_DIAG | 无 | 调试计数器（28 B） |

除 PING/状态/诊断外，SET 命令只写入 **暂存配置**；必须再发 `APPLY` 才改变实际 DDS 输出。

### 4.1 `00` — PING

请求 Payload：空。

应答 Payload（4 B）：

| 偏移 | 类型 | 含义 |
|---:|---|---|
| 0 | u8 | 协议版本，当前 `1` |
| 1 | u8 | 固件主版本，当前 `1` |
| 2 | u8 | 固件次版本，当前 `0` |
| 3 | u8 | 保留，当前 `0` |

### 4.2 `01` — GET_STATUS

请求 Payload：空。

应答 Payload 固定为 12 B，通道 `ch=0..3` 各占 3 B：

| 通道块内偏移 | 类型 | 含义 |
|---:|---|---|
| 0 | u8 | `mode` |
| 1 | u8 | `enabled`，0=关闭，1=启用 |
| 2 | u8 | 保留，0 |

`mode` 定义：`0=CW`、`1=FSK`、`2=ASK`、`5=QPSK`、`8=BPSK`、`9=4FSK`、`FF=OFF`。

### 4.3 `02` — SET_CW

Payload 固定 7 B：

| 偏移 | 类型 | 含义 |
|---:|---|---|
| 0 | u8 | `ch` |
| 1 | u32 LE | `freq_hz` |
| 5 | u16 LE | `asf` |

### 4.4 `03` — SET_FSK

Payload 固定 11 B：

| 偏移 | 类型 | 含义 |
|---:|---|---|
| 0 | u8 | `ch` |
| 1 | u32 LE | `f_mark_hz`，符号 bit=1 时输出 |
| 5 | u32 LE | `f_space_hz`，符号 bit=0 时输出 |
| 9 | u16 LE | `asf` |

### 4.5 `04` — SET_ASK

Payload 固定 9 B：

| 偏移 | 类型 | 含义 |
|---:|---|---|
| 0 | u8 | `ch` |
| 1 | u32 LE | `freq_hz` |
| 5 | u16 LE | `asf_on`，符号 bit=1 |
| 7 | u16 LE | `asf_off`，符号 bit=0 |

### 4.6 `05` — SET_BPSK

Payload 固定 11 B：

| 偏移 | 类型 | 含义 |
|---:|---|---|
| 0 | u8 | `ch` |
| 1 | u32 LE | `freq_hz` |
| 5 | u16 LE | `asf` |
| 7 | u16 LE | `phase0`，符号 bit=0 |
| 9 | u16 LE | `phase1`，符号 bit=1 |

### 4.7 `06` — SET_QPSK

Payload 固定 7 B：

| 偏移 | 类型 | 含义 |
|---:|---|---|
| 0 | u8 | `ch` |
| 1 | u32 LE | `freq_hz` |
| 5 | u16 LE | `asf` |

QPSK 的相位映射由当前固件固定，不能由此命令改写：

| 输入两 bit（先到的为高位） | CPOW | 相位 |
|---|---:|---:|
| `00` | `0x0800` | 45° |
| `01` | `0x1800` | 135° |
| `10` | `0x3800` | 315° |
| `11` | `0x2800` | 225° |

### 4.8 `07` — SET_4FSK

Payload 固定 19 B：

| 偏移 | 类型 | 含义 |
|---:|---|---|
| 0 | u8 | `ch` |
| 1 | u16 LE | `asf` |
| 3 | u32 LE | `f0_hz`，符号 `00` |
| 7 | u32 LE | `f1_hz`，符号 `01` |
| 11 | u32 LE | `f2_hz`，符号 `10` |
| 15 | u32 LE | `f3_hz`，符号 `11` |

### 4.9 `08` — SET_SYMBOLS

Payload：`bit_count(u16 LE) + bit_data[bit_count]`。

- `bit_count`：`1..256`，代表输入 bit 的数量，不是字节数换算后的 bit 数。
- 后续每一个输入 bit 占 **一个字节**；仅该字节的 bit0 有效，`0` 表示 bit 0，`1` 表示 bit 1。
- FSK、ASK、BPSK 每个符号消耗 1 个输入 bit。
- QPSK、4FSK 每个符号消耗连续 2 个输入 bit，先发送的 bit 为高位。例如 `0x01, 0x00` 对应符号 `10`。
- 当前符号缓冲区未消费完前，重复 SET_SYMBOLS 会收到 BUSY NACK。

建议时序：先发送所有 SET_xxx，发送 APPLY，收到 ACK 后再发送 SET_SYMBOLS。因为 APPLY 会清空尚未消费的符号缓冲区。

### 4.10 `09` — APPLY

请求 Payload：空。

将完整 `pending_cfg[0..3]` 复制至实时配置，写入静态 AD9959 寄存器并重建定时器发送帧。

应答 Payload 为 1 B 通道掩码：bit0..bit3 分别对应 CH0..CH3。该 ACK 表示“已受理”，实际切换在主循环中随后完成。

重要：暂存配置是完整的四通道镜像。若只发送一个通道的 SET 命令后就 APPLY，其他未设置通道将以默认关闭状态被覆盖。要保留的每个通道都必须在同一轮 APPLY 前重新发送其 SET 命令。

### 4.11 `0A` — STOP_OUTPUT

请求 Payload：空；应答为空 ACK。

当前固件行为是停止 TIM8 的后续寄存器更新并把软件通道标记为 OFF。**它没有向 AD9959 显式写入零幅度或 Power-Down，因此不能作为射频安全关断命令。** 如需可靠关断，请先以 `SET_CW(asf=0)` 配置目标通道并 APPLY；安全关断策略将在后续固件中单独完善。

### 4.12 `0B` — GET_DIAG

请求 Payload：空。应答 Payload 固定 28 B，均为 `u32 LE`：

| 偏移 | 字段 |
|---:|---|
| 0 | `init_stage` |
| 4 | `ad9959_frame_count` |
| 8 | `software_frame_count` |
| 12 | `spi1_error` |
| 16 | `spi1_dma_error` |
| 20 | `tx_running` |
| 24 | `tx_stop_pending` |

## 5. 应答与 NACK

成功 ACK：CMD 与请求相同、Payload 长度通常为 0。SET 命令与 STOP 均如此。

失败 NACK：应答 CMD = `请求 CMD | 0x80`，Payload 长度为 1，内容为错误码：

| 错误码 | 名称 | 含义 |
|---:|---|---|
| `01` | BAD_CRC | 保留；当前 CRC 错误帧会静默丢弃 |
| `02` | BAD_LEN | Payload 长度不足 |
| `03` | BAD_CMD | 未定义命令 |
| `04` | BAD_PARAM | 参数非法 |
| `05` | INVALID_CH | 通道不在 0..3 |
| `06` | BUSY | 符号缓冲区或 DDS 切换忙 |
| `07` | NOT_READY | 尚未设置暂存配置便请求 APPLY |

## 6. 当前功能边界与联调注意事项

1. 串口 SET 命令目前只覆盖 CW、FSK、ASK、BPSK、QPSK、4FSK。GFSK、MSK、AM、FM 没有对应 CMD。
2. 固件仅对 CW 的 `asf > 1023` 返回 BAD_PARAM；其余调制 SET 命令当前会接受超范围 ASF，编码时低 10 bit 生效。上位机必须自行限制 `0..1023`，后续固件应统一补充校验。
3. 固件以最小长度判断 SET 命令，多余 Payload 字节会被忽略。上位机应严格按本文规定长度发送。
4. `VER` 当前应填 `01`；固件当前未对版本字段回 NACK，因此上位机必须主动做版本兼容管理。
5. UART 接收帧没有独立事务超时；若中途断帧，下一帧应从 `A5 5A` 开始重新同步。

## 7. 上位机实现建议

推荐单线程、请求—应答方式：

1. 打开串口后发送 PING，确认 13 B 应答和 `SEQ` 一致。
2. 对每个需要启用的通道发送一个 SET_xxx，逐帧等待 ACK。
3. 发送 APPLY，检查 ACK 返回的通道掩码。
4. 对数字调制再发送 SET_SYMBOLS；根据调制类型用每符号 1 或 2 个字节提供输入 bit。
5. 需要诊断时发送 GET_DIAG；不要把 STOP_OUTPUT 当作硬件级射频切断。

## 8. 常见模式测试报文（CH1）

以下报文均已经按本协议计算 CRC，可直接以“十六进制发送”方式写入串口。示例采用 CH1（`ch=1`）、最大幅度 `asf=1023`、每条报文的 `SEQ` 递增。每次上电后建议先发 PING；每次 SET_xxx 后必须发 APPLY。

### 8.1 CW：CH1，99.7 MHz

```text
# SET_CW: ch=1, freq=99,700,000 Hz, asf=1023
A5 5A 01 01 02 07 00 01 20 4D F1 05 FF 03 0A BC

# APPLY
A5 5A 01 02 09 00 00 A4 C8
```

预期：第二条回 `A5 5A 01 02 09 01 00 02 ...`，其中最后的 Payload `02` 是 CH1 掩码（bit1）。

### 8.2 二进制 FSK：99.7 MHz / 100.3 MHz，发送 01010101

```text
# SET_FSK: ch=1, bit=1 -> 99.7 MHz, bit=0 -> 100.3 MHz, asf=1023
A5 5A 01 03 03 0B 00 01 20 4D F1 05 E0 74 FA 05 FF 03 32 CB

# APPLY
A5 5A 01 02 09 00 00 A4 C8

# SET_SYMBOLS: 8 个输入 bit，依次 0,1,0,1,0,1,0,1
A5 5A 01 04 08 0A 00 08 00 00 01 00 01 00 01 00 01 E8 74
```

注意：当前 FSK 定义中 `1=Mark=99.7 MHz`，`0=Space=100.3 MHz`。符号序列播放完后，固件会按其当前定时器链的默认生成逻辑继续产生符号；需要精确的一次性突发控制时，应在逻辑分析仪上验证实际行为。

### 8.3 二进制 ASK：99.7 MHz，开/关幅度

```text
# SET_ASK: ch=1, 99.7 MHz, bit=1 -> asf=1023, bit=0 -> asf=0
A5 5A 01 05 04 09 00 01 20 4D F1 05 FF 03 00 00 C3 A7

# APPLY
A5 5A 01 02 09 00 00 A4 C8

# 发送 01010101
A5 5A 01 04 08 0A 00 08 00 00 01 00 01 00 01 00 01 E8 74
```

### 8.4 BPSK：99.7 MHz，0° / 180°

```text
# SET_BPSK: ch=1, 99.7 MHz, asf=1023, bit=0 -> CPOW 0, bit=1 -> CPOW 0x2000
A5 5A 01 06 05 0B 00 01 20 4D F1 05 FF 03 00 00 00 20 26 5B

# APPLY
A5 5A 01 02 09 00 00 A4 C8

# 发送 01010101
A5 5A 01 04 08 0A 00 08 00 00 01 00 01 00 01 00 01 E8 74
```

### 8.5 QPSK：99.7 MHz，依次发送 00、01、10、11

```text
# SET_QPSK: ch=1, 99.7 MHz, asf=1023
A5 5A 01 07 06 07 00 01 20 4D F1 05 FF 03 CB E0

# APPLY
A5 5A 01 02 09 00 00 A4 C8

# SET_SYMBOLS: 8 个输入 bit = 00 01 10 11
# 每个 bit 均占一个字节，先发送的 bit 是 QPSK 符号高位
A5 5A 01 08 08 0A 00 08 00 00 00 00 01 01 00 01 01 F2 A8
```

### 8.6 4FSK：99.0 / 99.5 / 100.0 / 100.5 MHz

```text
# SET_4FSK: ch=1, asf=1023, 00/01/10/11 对应四个频率
A5 5A 01 09 07 13 00 01 FF 03 C0 9E E6 05 E0 3F EE 05 00 E1 F5 05 20 82 FD 05 07 EE

# APPLY
A5 5A 01 02 09 00 00 A4 C8

# SET_SYMBOLS: 00 01 10 11
A5 5A 01 08 08 0A 00 08 00 00 00 00 01 01 00 01 01 F2 A8
```

### 8.7 查询与停止

```text
# GET_STATUS, SEQ=0x0B
A5 5A 01 0B 01 00 00 72 92

# GET_DIAG, SEQ=0x0C
A5 5A 01 0C 0B 00 00 9E 04

# STOP_OUTPUT, SEQ=0x0A（注意：不是硬件安全关断）
A5 5A 01 0A 0A 00 00 37 14
```

Python CRC 参考实现：

```python
def crc16_ccitt_false(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if (crc & 0x8000) else (crc << 1) & 0xFFFF
    return crc

def make_frame(seq: int, cmd: int, payload: bytes = b"") -> bytes:
    head = bytes((0xA5, 0x5A, 0x01, seq & 0xFF, cmd & 0xFF,
                  len(payload) & 0xFF, len(payload) >> 8))
    crc = crc16_ccitt_false(head[2:] + payload)
    return head + payload + crc.to_bytes(2, "little")
```
