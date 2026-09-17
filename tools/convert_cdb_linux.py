#!/usr/bin/env python3
"""
把 Windows 上生成的 compile_commands.json 转换为 Linux 上 clangd 可用的版本。

仓库中的 compile_commands.json 在 Windows 生成，含 Windows 绝对路径
(directory: C:\\Users\\Shuchen Tao\\Documents\\DDS_9959_combd)，且编译驱动为
arm-none-eabi-gcc。Linux 本机没有 ARM GCC 工具链，clangd 解析不到这些参数。

转换内容:
  1. directory:  Windows 路径 -> Linux 工作区路径
  2. 编译驱动:   arm-none-eabi-gcc -> clang --target=arm-none-eabi
                 (clangd 内置 clang 前端，按 ARM 目标解析代码，无需真实 clang 可执行文件)

用法:
  python3 tools/convert_cdb_linux.py            # 就地转换仓库根目录的 compile_commands.json
  python3 tools/convert_cdb_linux.py [路径]      # 转换指定 compile_commands.json

幂等: 重复运行不会重复转换。
"""
import json
import os
import sys

WIN_BASE = r"C:\Users\Shuchen Tao\Documents\DDS_9959_combd"


def linux_base():
    """工作区根目录 = 脚本所在目录的上级目录。"""
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def convert_driver(args):
    """把编译驱动 arm-none-eabi-gcc 换成 clang --target=arm-none-eabi。"""
    for i, arg in enumerate(args):
        if "arm-none-eabi-gcc" in arg or (i == 0 and arg.endswith("gcc")):
            args[i] = "clang"
            # 仅在缺失时补 --target，保证幂等
            if "--target=arm-none-eabi" not in args:
                args.insert(i + 1, "--target=arm-none-eabi")
            return True
    return False


def convert_entry(entry, win_base, linux):
    entry["directory"] = entry["directory"].replace(win_base, linux)
    if "arguments" in entry:
        convert_driver(entry["arguments"])
    elif "command" in entry:
        cmd = entry["command"].replace(win_base, linux)
        if "arm-none-eabi-gcc" in cmd:
            cmd = cmd.replace("arm-none-eabi-gcc", "clang --target=arm-none-eabi", 1)
        entry["command"] = cmd
    return entry


def main():
    cdb_path = sys.argv[1] if len(sys.argv) > 1 else "compile_commands.json"
    if not os.path.isabs(cdb_path):
        cdb_path = os.path.join(os.getcwd(), cdb_path)
    linux = linux_base()
    with open(cdb_path, encoding="utf-8") as f:
        db = json.load(f)
    changed = [convert_entry(e, WIN_BASE, linux) for e in db]
    with open(cdb_path, "w", encoding="utf-8") as f:
        json.dump(changed, f, indent=1)
        f.write("\n")
    print(f"已转换 {len(changed)} 条编译命令 -> {cdb_path}")
    print(f"  directory: {WIN_BASE} -> {linux}")


if __name__ == "__main__":
    main()
