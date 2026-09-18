#!/usr/bin/env py
# -*- coding: utf-8 -*-
"""
host_cli.py — AD9959 SDT 上位机协议帧计算器（纯计算，无串口依赖）

只生成 HEX 帧字符串，粘贴到 XCOM/SSCOM（勾选 HEX 发送）即可；也能解码
串口助手收到的应答帧（验证 CRC、解析 ACK/NACK）。

帧格式: A5 5A 01 SEQ CMD LEN_L LEN_H PAYLOAD CRC16_L CRC16_H
        CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) 覆盖 VER..PAYLOAD。
见 DOCS/USART1_AD9959_Protocol.md。

用法（一次可给多个命令，按顺序生成; SEQ 自动递增, 可用 --seq 指定起始）:

  生成发送帧:
    py tools/host_cli.py ping
    py tools/host_cli.py set_cw 0 50000000 1023
    py tools/host_cli.py set_fsk 0 50005000 49995000 128
    py tools/host_cli.py fskc 0 50000000 5000 128     # 中心±频偏的 FSK 简写
    py tools/host_cli.py set_ask 0 50000000 1023 0
    py tools/host_cli.py set_bpsk 0 50000000 512 0 8192
    py tools/host_cli.py set_qpsk 0 50000000 512
    py tools/host_cli.py set_4fsk 0 512 45000000 48000000 51000000 54000000
    py tools/host_cli.py symbols 01011100             # payload 为 0/1 位串
    py tools/host_cli.py symbolsx 5C                  # 同上, HEX 字节按 MSB-first 展开
    py tools/host_cli.py repeat 1                     # 无限循环重发最近 payload
    py tools/host_cli.py get_status / get_diag / apply / stop
    py tools/host_cli.py --seq 5 fskc 0 50000000 5000 128 apply symbols 01011100

  解码应答帧 (把串口助手 HEX 显示的内容粘进来):
    py tools/host_cli.py decode A5 5A 01 00 00 04 00 01 01 00 00 74 B4

  常用流程:  fskc 0 50000000 5000 128 apply symbols 01011100
  循环重发:  symbols 01011100 repeat 1  （repeat 0 / stop 停止；apply 取消）
  注意: 固件 debug_manual_enable=1 时 apply/stop/repeat 会被回 NACK 0x08。
"""

import argparse
import sys

# ---- 协议常量 (与 App/host_protocol.h 一致) --------------------------------
SOF1, SOF2, VER = 0xA5, 0x5A, 0x01

CMD_PING, CMD_GET_STATUS, CMD_SET_CW, CMD_SET_FSK = 0x00, 0x01, 0x02, 0x03
CMD_SET_ASK, CMD_SET_BPSK, CMD_SET_QPSK, CMD_SET_4FSK = 0x04, 0x05, 0x06, 0x07
CMD_SET_SYMBOLS, CMD_APPLY, CMD_STOP_OUTPUT, CMD_GET_DIAG = 0x08, 0x09, 0x0A, 0x0B
CMD_SET_REPEAT = 0x0C

NACK_NAMES = {
    0x00: "OK", 0x01: "BAD_CRC", 0x02: "BAD_LEN", 0x03: "BAD_CMD",
    0x04: "BAD_PARAM", 0x05: "INVALID_CH", 0x06: "BUSY",
    0x07: "NOT_READY", 0x08: "LOCAL_MODE(手动调试模式占用)",
}

CMD_NAMES = {v: k for k, v in globals().items() if k.startswith("CMD_")}


def crc16(buf):
    crc = 0xFFFF
    for b in buf:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def build_frame(cmd, payload=b"", seq=0):
    body = bytes([VER, seq & 0xFF, cmd, len(payload) & 0xFF, (len(payload) >> 8) & 0xFF]) + payload
    c = crc16(body)
    return bytes([SOF1, SOF2]) + body + bytes([c & 0xFF, (c >> 8) & 0xFF])


# ---- 命令表: 名字 -> (argc, 构造函数) ---------------------------------------
def _payload_set_cw(a):
    ch, freq, asf = int(a[0], 0), int(a[1], 0), int(a[2], 0)
    return bytes([ch]) + freq.to_bytes(4, "little") + asf.to_bytes(2, "little"), \
           f"ch{ch} freq={freq} asf={asf}"


def _payload_set_fsk(a):
    ch, fm, fs, asf = int(a[0], 0), int(a[1], 0), int(a[2], 0), int(a[3], 0)
    return bytes([ch]) + fm.to_bytes(4, "little") + fs.to_bytes(4, "little") \
           + asf.to_bytes(2, "little"), f"ch{ch} mark={fm} space={fs} asf={asf}"


def _payload_fskc(a):
    ch, center, dev, asf = int(a[0], 0), int(a[1], 0), int(a[2], 0), int(a[3], 0)
    return _payload_set_fsk([a[0], str(center + dev), str(center - dev), a[3]])


def _payload_set_ask(a):
    ch, freq, on, off = int(a[0], 0), int(a[1], 0), int(a[2], 0), int(a[3], 0)
    return bytes([ch]) + freq.to_bytes(4, "little") + on.to_bytes(2, "little") \
           + off.to_bytes(2, "little"), f"ch{ch} freq={freq} asf_on={on} asf_off={off}"


def _payload_set_bpsk(a):
    ch, freq, asf, p0, p1 = (int(x, 0) for x in a)
    return bytes([ch]) + freq.to_bytes(4, "little") + asf.to_bytes(2, "little") \
           + p0.to_bytes(2, "little") + p1.to_bytes(2, "little"), \
           f"ch{ch} freq={freq} asf={asf} ph0={p0} ph1={p1}"


def _payload_set_qpsk(a):
    ch, freq, asf = int(a[0], 0), int(a[1], 0), int(a[2], 0)
    return bytes([ch]) + freq.to_bytes(4, "little") + asf.to_bytes(2, "little"), \
           f"ch{ch} freq={freq} asf={asf}"


def _payload_set_4fsk(a):
    ch, asf = int(a[0], 0), int(a[1], 0)
    fs = [int(x, 0) for x in a[2:]]
    return bytes([ch]) + asf.to_bytes(2, "little") + b"".join(
        f.to_bytes(4, "little") for f in fs), f"ch{ch} asf={asf} f={fs}"


def _payload_symbols(a):
    # 协议约定: 一字节一个 bit (仅 bit0 有效)。输入为 0/1 位串。
    s = a[0]
    if (len(s) == 0) or (not all(c in "01" for c in s)):
        raise ValueError("symbols 需要 0/1 位串, 如: symbols 01011100")
    nbits = len(s)
    if nbits > 62:
        print(f"[警告] {nbits} bit 超过单帧上限 62 bit (HOST_PAYLOAD_MAX=64)，固件会回 NACK BAD_LEN")
    data = bytes((0x01 if c == "1" else 0x00) for c in s)
    return (nbits).to_bytes(2, "little") + data, f"{nbits} bits [{s}]"


def _payload_symbols_hex(a):
    # 便捷版: HEX 字节串, 每字节按 MSB-first 展开为 8 个 bit。
    raw = bytes.fromhex(a[0])
    s = "".join("1" if (b & m) else "0" for b in raw
                for m in (0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01))
    return _payload_symbols([s])


def _payload_repeat(a):
    v = int(a[0], 0)
    if v not in (0, 1):
        raise ValueError("repeat 参数只能是 0(关) 或 1(开)")
    return bytes([v]), f"{'开' if v else '关'}"


def _payload_empty(a):
    return b"", ""


COMMANDS = {
    "ping":       (0, CMD_PING,       _payload_empty),
    "get_status": (0, CMD_GET_STATUS, _payload_empty),
    "get_diag":   (0, CMD_GET_DIAG,   _payload_empty),
    "apply":      (0, CMD_APPLY,      _payload_empty),
    "stop":       (0, CMD_STOP_OUTPUT, _payload_empty),
    "set_cw":     (3, CMD_SET_CW,     _payload_set_cw),
    "set_fsk":    (4, CMD_SET_FSK,    _payload_set_fsk),
    "fskc":       (4, CMD_SET_FSK,    _payload_fskc),
    "set_ask":    (4, CMD_SET_ASK,    _payload_set_ask),
    "set_bpsk":   (5, CMD_SET_BPSK,   _payload_set_bpsk),
    "set_qpsk":   (3, CMD_SET_QPSK,   _payload_set_qpsk),
    "set_4fsk":   (6, CMD_SET_4FSK,   _payload_set_4fsk),
    "symbols":    (1, CMD_SET_SYMBOLS, _payload_symbols),
    "symbolsx":   (1, CMD_SET_SYMBOLS, _payload_symbols_hex),
    "repeat":     (1, CMD_SET_REPEAT, _payload_repeat),
}


def emit_frames(tokens, seq_start):
    """顺序解析命令串, 打印带编号的 HEX 帧。返回发送帧数。"""
    i, n, seq = 0, 0, seq_start
    while i < len(tokens):
        name = tokens[i].lower()
        if name == "decode":
            sys.exit("decode 只能单独使用")
        if name not in COMMANDS:
            sys.exit(f"未知命令: {tokens[i]}\n可用: {' '.join(sorted(COMMANDS))} + decode")
        argc, cmd, builder = COMMANDS[name]
        if i + 1 + argc > len(tokens):
            sys.exit(f"命令 {name} 需要 {argc} 个参数, 只剩 {len(tokens) - i - 1} 个")
        payload, desc = builder(tokens[i + 1:i + 1 + argc])
        frame = build_frame(cmd, payload, seq)
        n += 1
        seq = (seq + 1) & 0xFF
        print(f"# {n:2d}  {name}{' ' + desc if desc else ''}")
        print(frame.hex(' ').upper())
        i += 1 + argc
    return n


def decode_frame(hexstr):
    """解码一串 HEX (可带空格) 为一个应答帧并校验。"""
    h = "".join(hexstr.split())
    try:
        data = bytes.fromhex(h)
    except ValueError:
        sys.exit("HEX 解析失败: 只接受十六进制字节, 可带空格")
    if len(data) < 9:
        sys.exit(f"帧太短: 至少 9 字节 (帧头 7 + CRC 2), 收到 {len(data)} 字节")
    if data[0] != SOF1 or data[1] != SOF2:
        sys.exit(f"不是有效帧: 前两字节应为 A5 5A, 收到 {data[:2].hex(' ').upper()}")
    ver, seq, cmd = data[2], data[3], data[4]
    plen = data[5] | (data[6] << 8)
    if len(data) != 9 + plen:
        sys.exit(f"长度不符: 帧头声明 payload={plen}B, 但总长 {len(data)}B 应为 {9 + plen}B")
    payload = data[7:7 + plen]
    crc_got = data[7 + plen] | (data[8 + plen] << 8)
    crc_exp = crc16(data[2:7 + plen])
    print(f"VER={ver:02X} SEQ={seq:02X} CMD={cmd:02X}"
          f" ({CMD_NAMES.get(cmd, '?')}) len={plen} CRC={'OK' if crc_got == crc_exp else f'错误(应 {crc_exp:04X})'}")
    if cmd & 0x80:
        reason = payload[0] if payload else 0xFF
        print(f"→ NACK, 原因: {NACK_NAMES.get(reason, reason)} ({reason:02X})")
    else:
        print(f"→ ACK, payload = {payload.hex(' ').upper() if payload else '(空)'}")


def main():
    ap = argparse.ArgumentParser(description="AD9959 SDT 协议帧计算器 (纯计算, 不碰串口)")
    ap.add_argument("--seq", type=int, default=1, help="起始 SEQ 字节 (默认 1, 批量自动递增)")
    ap.add_argument("tokens", nargs="+", help="命令序列, 如: fskc 0 50000000 5000 128 apply symbols 5C")
    a = ap.parse_args()

    if a.tokens[0].lower() == "decode":
        decode_frame(" ".join(a.tokens[1:]))
        return
    if len(a.tokens) == 1 and a.tokens[0].lower() == "decode":
        sys.exit("decode 需要一个 HEX 参数, 如: decode A5 5A 01 00 00 04 00 01 01 00 00 74 B4")

    n = emit_frames(a.tokens, a.seq)
    if n:
        print(f"\n# 共 {n} 帧。XCOM 勾选 HEX 发送, 逐行粘贴。"
              "注意 debug_manual_enable=1 时 apply/stop 会被 NACK 0x08。")


if __name__ == "__main__":
    main()
