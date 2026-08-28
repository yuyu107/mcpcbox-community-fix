#!/usr/bin/env python3
"""Apply the v6.2 compatibility patches to the v5.6 MCPCBox executable."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path


INPUT_SHA256 = "f27553d47e09970cf15b66808def12bec9d57ace19d6cd2d55117db9fca9ff5f"
OUTPUT_SHA256 = "85441aa612691039bcc84e9c095beb7cb95ad6b63ba7fd5b10132faf02c157fc"

PATCHES = (
    (
        "http://pkg.tuboshu.com/common/mc/pc/game/1464848531742/Java_7_8_V64.7z",
        "https://raw.githubusercontent.com/yuyu107/mcpcbox-downloads/main/j.7z",
    ),
    (
        "0aae6a0b8a2049abc80afa193bd64ecc",
        "a8bc19564e9bbebba302a4e95413425d",
    ),
    (
        "https://s3.amazonaws.com/Minecraft.Download/versions/",
        "https://yuyu107.github.io/mcpcbox-downloads/",
    ),
)

ASCII_PATCHES = (
    (
        "http://webmcbox.duowan.com/s/pc/wonderfulvideo/videoIndex.html",
        "https://yuyu107.github.io/mcpcbox-downloads/video/index.html",
        3,
    ),
    (
        "http://webmcbox.duowan.com/s/pcv3/index.html",
        "https://yuyu107.github.io/mcpcbox-downloads/",
        5,
    ),
)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def padded_utf16(old: str, new: str) -> tuple[bytes, bytes]:
    old_bytes = old.encode("utf-16le")
    new_bytes = new.encode("utf-16le")
    if len(new_bytes) > len(old_bytes):
        raise ValueError(f"replacement is too long: {new!r}")
    return old_bytes, new_bytes + b"\0" * (len(old_bytes) - len(new_bytes))


def patch(input_path: Path, output_path: Path) -> None:
    data = input_path.read_bytes()
    actual_input = sha256(data)
    if actual_input != INPUT_SHA256:
        raise SystemExit(
            "输入文件不是受支持的 v5.6 MCPCBox.exe\n"
            f"期望 SHA-256: {INPUT_SHA256}\n实际 SHA-256: {actual_input}"
        )

    for old, new in PATCHES:
        old_bytes, replacement = padded_utf16(old, new)
        count = data.count(old_bytes)
        if count != 1:
            raise SystemExit(f"补丁目标出现次数应为 1，实际为 {count}: {old}")
        data = data.replace(old_bytes, replacement)

    for old, new, expected_count in ASCII_PATCHES:
        old_bytes = old.encode("ascii")
        new_bytes = new.encode("ascii")
        if len(new_bytes) > len(old_bytes):
            raise SystemExit(f"ASCII 替换内容过长: {new}")
        count = data.count(old_bytes)
        if count != expected_count:
            raise SystemExit(
                f"补丁目标出现次数应为 {expected_count}，实际为 {count}: {old}"
            )
        replacement = new_bytes + b"\0" * (len(old_bytes) - len(new_bytes))
        data = data.replace(old_bytes, replacement)

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
    parser = argparse.ArgumentParser(description="生成 MCPCBox Community Fix v6.2 主程序")
    parser.add_argument("input", type=Path, help="v5.6 MCPCBox.exe")
    parser.add_argument("output", type=Path, help="输出 MCPCBox.exe")
    args = parser.parse_args()
    patch(args.input, args.output)


if __name__ == "__main__":
    main()
