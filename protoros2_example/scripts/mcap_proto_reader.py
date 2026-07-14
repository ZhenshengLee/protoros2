#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
mcap_proto_reader.py
使用纯 Python Mapped-Data 生态 (mcap, protobuf, rosbags) 遍历和解析 MCAP 格式的 bag 包。
不依赖 ROS 2 运行时环境，适配云端/离线开发。
"""

import argparse
import sys
import os
import tempfile
import subprocess
from mcap.reader import make_reader

# 尝试导入 rosbags 作为纯 Python CDR 反序列化包，做平滑兼容
try:
    from rosbags.typesys import get_typestore, Stores, get_types_from_msg

    HAS_ROSBAGS = True
except ImportError:
    HAS_ROSBAGS = False


def find_package_share_dir(package_name: str) -> str:
    """寻找 ROS 2 package share 目录候选路径"""
    search_dirs = [
        f"/opt/ros/lyrical/share/{package_name}",
        f"/gw_demo/target/colcon/install/share/{package_name}",
        f"./target/colcon/install/share/{package_name}",
        f"../target/colcon/install/share/{package_name}",
        f"target/colcon/install/share/{package_name}",
    ]
    for candidate in search_dirs:
        if os.path.isdir(candidate):
            return candidate
    return None


class DynamicProtoDecoder:
    def __init__(self):
        self.msg_classes = {}

    def get_message_class(self, topic_type: str):
        if topic_type in self.msg_classes:
            return self.msg_classes[topic_type]

        parts = topic_type.split("/")
        if len(parts) != 3:
            raise ValueError(f"Invalid topic type: {topic_type}")
        package_name, _, message_name = parts

        share_dir = find_package_share_dir(package_name)
        if not share_dir:
            raise FileNotFoundError(f"未找到 package '{package_name}' 的安装共享目录")

        proto_path = os.path.join(share_dir, "msg", f"{message_name}.proto")
        if not os.path.exists(proto_path):
            raise FileNotFoundError(f"未找到对应的 .proto 描述文件: {proto_path}")

        # 动态编译 proto
        temp_dir = tempfile.gettempdir()
        share_parent_dir = os.path.dirname(share_dir)

        # 执行 protoc
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

    def has_proto(self, topic_type: str) -> bool:
        parts = topic_type.split("/")
        if len(parts) != 3:
            return False
        package_name, _, message_name = parts
        share_dir = find_package_share_dir(package_name)
        if not share_dir:
            return False
        proto_path = os.path.join(share_dir, "msg", f"{message_name}.proto")
        return os.path.exists(proto_path)

    def decode(self, topic_type: str, data: bytes):
        msg_class = self.get_message_class(topic_type)
        msg = msg_class()
        msg.ParseFromString(data)
        return msg


def read_pure_mcap(mcap_path: str):
    if not os.path.exists(mcap_path):
        print(f"[Error] 指定的 MCAP 文件路径不存在: {mcap_path}", file=sys.stderr)
        sys.exit(1)

    print(f"=== [mcap_proto] 开始使用纯 Python MLOps 工具解析 MCAP 文件: {mcap_path} ===")

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

            # 智能兼容中间件 Fallback 模式：当录制时默认打标为 protobuf，但在 Share 目录未发现 .proto 描述文件时，自动转为 CDR 反序列化
            effective_encoding = encoding
            if encoding == "protobuf" and not proto_decoder.has_proto(topic_type):
                effective_encoding = "cdr"

            if effective_encoding == "protobuf":
                try:
                    pb_msg = proto_decoder.decode(topic_type, message.data)
                    if hasattr(pb_msg, "message"):
                        print(f"    -> [Protobuf 纯生解析成功] message='{pb_msg.message}'")
                    else:
                        fields = {fd.name: getattr(pb_msg, fd.name) for fd in pb_msg.DESCRIPTOR.fields}
                        print(f"    -> [Protobuf 纯生解析成功] {fields}")
                except Exception as e:
                    print(f"    -> [Protobuf 纯生解析失败] {e}", file=sys.stderr)

            elif effective_encoding == "cdr":
                if HAS_ROSBAGS:
                    try:
                        typestore = get_typestore(Stores.LATEST)
                        if topic_type not in typestore.types:
                            parts = topic_type.split("/")
                            if len(parts) == 3:
                                package_name, _, message_name = parts
                                share_dir = find_package_share_dir(package_name)
                                if share_dir:
                                    msg_path = os.path.join(share_dir, "msg", f"{message_name}.msg")
                                    if os.path.exists(msg_path):
                                        with open(msg_path, "r", encoding="utf-8") as msg_f:
                                            msg_text = msg_f.read()
                                        typesdict = get_types_from_msg(msg_text, topic_type)
                                        typestore.register(typesdict)
                        ros_msg = typestore.deserialize_cdr(message.data, topic_type)
                        if hasattr(ros_msg, "message"):
                            print(f"    -> [CDR rosbags 纯生解析成功] message='{ros_msg.message}'")
                        else:
                            print(f"    -> [CDR rosbags 纯生解析成功] {ros_msg}")
                    except Exception as e:
                        print(f"    -> [CDR 纯生解析失败] {e}", file=sys.stderr)
                else:
                    print(f"    -> [CDR 纯生解析，未装 rosbags，降级仅打印大小: {len(message.data)} 字节]")
            else:
                print("    -> (未知编码格式，略过解码)")

    print(f"\n=== [mcap_proto] 读取完成！共成功处理 {msg_count} 条消息 ===")


def main():
    parser = argparse.ArgumentParser(description="使用纯 Python MLOps 工具链解析 MCAP 数据包，不依赖 ROS 2 环境。")
    parser.add_argument("mcap_path", type=str, help="欲解析的 .mcap 文件的路径")
    args = parser.parse_args()

    read_pure_mcap(args.mcap_path)


if __name__ == "__main__":
    main()
