#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
mcap_ros2_reader.py
使用 ROS 2 生态 (mcap, rosidl_runtime_py, rclpy) 读取和解析 MCAP 格式的 bag 包。
支持自适应解析 CDR 与 Protobuf 序列化消息。
"""

import argparse
import sys
import os
import tempfile
import subprocess
import importlib  # noqa: F401
from mcap.reader import make_reader

# 尝试导入 ROS 2 工具依赖
try:
    from rosidl_runtime_py.utilities import get_message
    from rclpy.serialization import deserialize_message
    from ament_index_python.packages import get_package_share_directory
except ImportError:
    print(
        "[Error] 未找到 ROS 2 Python 运行时依赖。请确保已 source setup.bash 并处于 ROS 2 开发环境中。", file=sys.stderr
    )
    sys.exit(1)


class DynamicProtoDecoder:
    def __init__(self):
        self.msg_classes = {}

    def get_message_class(self, topic_type: str):
        if topic_type in self.msg_classes:
            return self.msg_classes[topic_type]

        # 期望格式: "protoros2_example/msg/ExampleMessage"
        parts = topic_type.split("/")
        if len(parts) != 3:
            raise ValueError(f"Invalid topic type: {topic_type}")
        package_name, _, message_name = parts

        try:
            share_dir = get_package_share_directory(package_name)
        except Exception:
            # Fallback
            share_dir = f"/gw_demo/target/colcon/install/share/{package_name}"
            if not os.path.exists(share_dir):
                share_dir = f"./target/colcon/install/share/{package_name}"

        proto_path = os.path.join(share_dir, "msg", f"{message_name}.proto")
        if not os.path.exists(proto_path):
            raise FileNotFoundError(f"未找到对应的 .proto 描述文件: {proto_path}")

        # 动态编译 proto
        temp_dir = tempfile.gettempdir()
        share_parent_dir = os.path.dirname(share_dir)

        # 执行 protoc 编译生成 python 反序列化模块
        cmd = ["protoc", f"--python_out={temp_dir}", f"--proto_path={share_parent_dir}", proto_path]
        subprocess.check_call(cmd)

        py_filepath = os.path.join(temp_dir, package_name, "msg", f"{message_name}_pb2.py")
        if not os.path.exists(py_filepath):
            raise FileNotFoundError(f"编译生成的 Python 模块不存在: {py_filepath}")

        # 使用 SourceFileLoader 绕过 python namespace 包污染强行加载
        from importlib.machinery import SourceFileLoader

        module = SourceFileLoader(f"{message_name}_pb2", py_filepath).load_module()

        msg_class = getattr(module, message_name)
        self.msg_classes[topic_type] = msg_class
        return msg_class

    def decode(self, topic_type: str, data: bytes):
        msg_class = self.get_message_class(topic_type)
        msg = msg_class()
        msg.ParseFromString(data)
        return msg


def read_mcap_bag(mcap_path: str):
    if not os.path.exists(mcap_path):
        print(f"[Error] 指定的 MCAP 文件路径不存在: {mcap_path}", file=sys.stderr)
        sys.exit(1)

    print(f"=== [mcap_ros2] 开始解析与解码 MCAP 文件: {mcap_path} ===")

    proto_decoder = DynamicProtoDecoder()
    msg_count = 0

    with open(mcap_path, "rb") as f:
        reader = make_reader(f)
        for schema, channel, message in reader.iter_messages():
            msg_count += 1
            topic_name = channel.topic
            topic_type = schema.name if schema else "unknown"
            encoding = channel.message_encoding

            print(
                f"[{msg_count}] Topic: {topic_name} | Time: {message.log_time} ns | "
                f"Size: {len(message.data)} bytes | Encoding: '{encoding}'"
            )

            # 自适应解析
            if encoding == "protobuf":
                try:
                    pb_msg = proto_decoder.decode(topic_type, message.data)
                    # 尝试读取和打印消息字段
                    if hasattr(pb_msg, "message"):
                        print(f"    -> [Protobuf 解码成功] message='{pb_msg.message}'")
                    else:
                        # 展平打印
                        fields = {fd.name: getattr(pb_msg, fd.name) for fd in pb_msg.DESCRIPTOR.fields}
                        print(f"    -> [Protobuf 解码成功] {fields}")
                except Exception as e:
                    print(f"    -> [Protobuf 解码失败] {e}", file=sys.stderr)

            elif encoding == "cdr":
                try:
                    msg_class = get_message(topic_type)
                    if msg_class:
                        ros_msg = deserialize_message(message.data, msg_class)
                        if hasattr(ros_msg, "message"):
                            print(f"    -> [CDR 解码成功] message='{ros_msg.message}'")
                        else:
                            print(f"    -> [CDR 解码成功] {ros_msg}")
                except Exception as e:
                    print(f"    -> [CDR 解码失败] {e}", file=sys.stderr)
            else:
                print("    -> (未知编码格式，略过解码)")

    print(f"\n=== [mcap_ros2] 读取完成！共成功处理 {msg_count} 条消息 ===")


def main():
    parser = argparse.ArgumentParser(description="使用 ROS 2 生态与 MCAP API 读取并还原数据包。")
    parser.add_argument("mcap_path", type=str, help="欲解析的 .mcap 文件的路径")
    args = parser.parse_args()

    read_mcap_bag(args.mcap_path)


if __name__ == "__main__":
    main()
