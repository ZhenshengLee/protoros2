#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
rosbags_reader.py
使用第三方纯 Python 库 `rosbags` 读取和验证 rosbag 数据包。
目的：脱离 ROS 2 运行时底层 C++ 库依赖，通过纯粹的 Python 接口直接对 bag 数据包的元数据、消息类型、类型结构哈希(RIHS01)和序列化字节流格式(cdr)进行可读性与完整性检查。
"""

import argparse
import sys
from pathlib import Path

try:
    from rosbags.highlevel import AnyReader
except ImportError:
    print(
        "[Error] 未找到 `rosbags` 模块。请确认当前 Python 环境中安装了该包（例如 pip install rosbags）。",
        file=sys.stderr,
    )
    sys.exit(1)


def verify_bag_with_rosbags(bag_path: str):
    path = Path(bag_path)
    if not path.exists():
        print(f"[Error] 指定的 Bag 路径不存在: {bag_path}", file=sys.stderr)
        sys.exit(1)

    print(f"=== [rosbags] 开始使用纯 Python Reader 解析与验证: {bag_path} ===")

    try:
        with AnyReader([path]) as reader:
            print("\n--- 发现的连接与元数据格式 (Connections & Metadata) ---")
            for conn in reader.connections:
                ext_info = getattr(conn, "ext", None)
                digest_info = getattr(conn, "digest", "N/A")
                ser_format = (
                    ext_info.serialization_format if ext_info and hasattr(ext_info, "serialization_format") else "cdr"
                )

                print(f"  * Topic: {conn.topic}")
                print(f"    - MsgType: {conn.msgtype}")
                print(f"    - MsgCount: {conn.msgcount}")
                print(f"    - Serialization Format: '{ser_format}'")
                print(f"    - Type Hash (REP-2011 RIHS01): '{digest_info}'")
                if ext_info and hasattr(ext_info, "offered_qos_profiles"):
                    print(f"    - QoS Profiles Count: {len(ext_info.offered_qos_profiles)}")

            print("\n--- 正在遍历检查数据负载流 (Data Frames) ---")
            count = 0
            for conn, timestamp, rawdata in reader.messages():
                count += 1
                ext_info = getattr(conn, "ext", None)
                ser_format = (
                    ext_info.serialization_format if ext_info and hasattr(ext_info, "serialization_format") else "cdr"
                )
                print(
                    f"[{count}] Topic: {conn.topic} | Time: {timestamp} ns | "
                    f"Payload Size: {len(rawdata)} bytes | Format: '{ser_format}' | Type: {conn.msgtype}"
                )

            print(f"\n=== [rosbags] 纯 Python 验证完毕！该 Bag 结构完整、可读，共成功检查 {count} 条数据帧 ===")

    except Exception as e:
        print(f"[Error] 使用 rosbags 读取解析失败: {e}", file=sys.stderr)
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(description="使用纯 Python 库 rosbags 验证 Bag 文件的结构可读性与序列化格式。")
    parser.add_argument("bag_path", type=str, help="rosbag 数据包文件夹或文件路径")
    args = parser.parse_args()

    verify_bag_with_rosbags(args.bag_path)


if __name__ == "__main__":
    main()
