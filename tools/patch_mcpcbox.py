#!/usr/bin/env python3
"""Apply the MCPCBox Community Fix v6.3 patches to the supported v5.6 executable."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path


INPUT_SHA256 = "f27553d47e09970cf15b66808def12bec9d57ace19d6cd2d55117db9fca9ff5f"
OUTPUT_SHA256 = "115ab85504084afa7b080400750b48205402835153f35cd93063284698a2cd7a"

# UTF-16LE strings. Shorter replacements are padded with NUL bytes inside the
# original fixed-size string slot.
UTF16_PATCHES = (
    (
        "http://pkg.tuboshu.com/common/mc/pc/game/1464848531742/Java_7_8_V64.7z",
        "https://raw.githubusercontent.com/yuyu107/mcpcbox-downloads/main/j.7z",
        1,
    ),
    (
        "0aae6a0b8a2049abc80afa193bd64ecc",
        "a8bc19564e9bbebba302a4e95413425d",
        1,
    ),
    (
        "https://s3.amazonaws.com/Minecraft.Download/versions/",
        "https://yuyu107.github.io/mcpcbox-downloads/",
        1,
    ),
    (
        "http://tieba.duowan.com/f/minecraft.html?mcbox",
        "https://tieba.baidu.com/f?kw=多玩我的世界盒子",
        1,
    ),
)

# ASCII strings embedded in the UI XML / page configuration.
ASCII_PATCHES = (
    (
        "http://webmcbox.duowan.com/s/pc/wonderfulvideo/videoIndex.html",
        "https://yuyu107.github.io/mcpcbox-downloads/video/index.html",
        3,
        False,
    ),
    (
        "http://webmcbox.duowan.com/s/pcv3/index.html",
        "https://yuyu107.github.io/mcpcbox-downloads/",
        5,
        False,
    ),
    # Keep this replacement exactly the same byte length. The string also
    # occurs inside embedded UI XML, so inserting a NUL here would truncate it.
    (
        "http://hd.huya.com/mcbox/index.html",
        "https://yuyu107.github.io/web/live/",
        5,
        True,
    ),
)

# UTF-8 text embedded in the About dialog. These replacements are deliberately
# byte-for-byte equal in length to avoid shifting the resource data.
UTF8_EXACT_PATCHES = (
    (
        "版权所有 (c) 2014~2016 多玩我的世界盒子 保留一切权利",
        "原版版权 (c) 2014~2016 多玩我的世界盒子 保留一切权利",
        2,
    ),
    (
        "最屌的我的世界盒子",
        "2026 社区兼容修复版 ",
        2,
    ),
)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def padded(old: bytes, new: bytes) -> bytes:
    if len(new) > len(old):
        raise ValueError("replacement is longer than the original slot")
    return new + b"\0" * (len(old) - len(new))


def replace_checked(data: bytes, old: bytes, new: bytes, expected_count: int) -> bytes:
    count = data.count(old)
    if count != expected_count:
        raise SystemExit(
            f"补丁目标出现次数应为 {expected_count}，实际为 {count}: {old!r}"
        )
    return data.replace(old, new)


def patch(input_path: Path, output_path: Path) -> None:
    data = input_path.read_bytes()
    actual_input = sha256(data)
    if actual_input != INPUT_SHA256:
        raise SystemExit(
            "输入文件不是受支持的 MCPCBox v5.6 主程序\n"
            f"期望 SHA-256: {INPUT_SHA256}\n实际 SHA-256: {actual_input}"
        )

    for old, new, expected_count in UTF16_PATCHES:
        old_bytes = old.encode("utf-16le")
        new_bytes = new.encode("utf-16le")
        data = replace_checked(
            data,
            old_bytes,
            padded(old_bytes, new_bytes),
            expected_count,
        )

    for old, new, expected_count, must_equal_length in ASCII_PATCHES:
        old_bytes = old.encode("ascii")
        new_bytes = new.encode("ascii")
        if must_equal_length:
            if len(new_bytes) != len(old_bytes):
                raise SystemExit(f"ASCII 等长替换长度错误: {new}")
            replacement = new_bytes
        else:
            replacement = padded(old_bytes, new_bytes)
        data = replace_checked(data, old_bytes, replacement, expected_count)

    for old, new, expected_count in UTF8_EXACT_PATCHES:
        old_bytes = old.encode("utf-8")
        new_bytes = new.encode("utf-8")
        if len(new_bytes) != len(old_bytes):
            raise SystemExit(f"UTF-8 等长替换长度错误: {new}")
        data = replace_checked(data, old_bytes, new_bytes, expected_count)

    actual_output = sha256(data)
    if actual_output != OUTPUT_SHA256:
        raise SystemExit(
            "补丁输出校验失败\n"
            f"期望 SHA-256: {OUTPUT_SHA256}\n实际 SHA-256: {actual_output}"
        )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(data)
    print(f"补丁完成: {output_path}")
    print(f"SHA-256: {actual_output}")


def main() -> None:
    parser = argparse.ArgumentParser(description="生成 MCPCBox Community Fix v6.3 主程序")
    parser.add_argument("input", type=Path, help="受支持的 v5.6 MCPCBox.exe")
    parser.add_argument("output", type=Path, help="输出 MCPCBox.exe")
    args = parser.parse_args()
    patch(args.input, args.output)


if __name__ == "__main__":
    main()
