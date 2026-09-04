# protoros2

如需英文版本，请参阅 [English](README.md)。

## 简介 (Introduction)

对 ROS 2 的非侵入式（zero-intrusive）Protobuf 原生支持扩展。

### 动机 (Motivation)

- 2017 年，[Apollo 开发者在 ROS 社区建议在 ROS 1 (ros_comm) 中引入 Protobuf](https://github.com/ros/ros_comm/issues/1085)。
- 2013 年，ROS 2 早期设计提案 [ROS2 Message Research](https://design.ros2.org/articles/serialization.html) 即指出：序列化引擎（Serialization）应当是可选且可替换的，并应尽可能直接复用业界成熟的第三方库。
- ROS 2 的核心架构脱胎于设计文档 [ROS on DDS](https://design.ros2.org/articles/ros_on_dds.html)，但当下自动驾驶与机器人领域呈现出明确趋势：像 Iceoryx、eCAL 以及 Zenoh 等序列化无关（serialization-agnostic）的现代中间件越来越受到青睐。
- eProsima 早期发布的对比报告 [Apache Thrift vs Protocol Buffers vs Fast Buffers](https://www.eprosima.com/developer-resources/performance/apache-thrift-vs-protocol-buffers-vs-fast-buffers) 曾指出 FastCDR 在 pubsub benchmark 表现优于早期的 Protobuf (v2.5)；然而近年来 Protobuf 陆续推出了大量底层高性能特性，包括 [Protobuf: Arenas 内存池](https://protobuf.dev/reference/cpp/arenas/)、[Protobuf: String View APIs](https://protobuf.dev/reference/cpp/string-view/)、[Protobuf: ctype=CORD](https://protobuf.dev/news/2023-04-11/) 以及 [Protobuf: New RepeatedPtrField Layout](https://protobuf.dev/news/2025-09-19/#cpp-repeatedptrfield-layout) 等。
- Protobuf 由 Google 主导并长期维护，在整个 AI 工业界与云端数据闭环生态中拥有无可撼动的基准地位。
- Protobuf 具备天然的传统工程优势：接口极其易用、向后/向前版本演进友好、丰富的跨语言生态、车端（Edge）/云端（Cloud）/MCU 嵌入式全栈互通。
- 大量主流自动驾驶（SDV）与机器人团队的自研中间件均深度基于 Protobuf 构建（如 Waymo、Horizon 等）。

### 相关工作 (Related Work)

在 ROS 1 中使用 Protobuf：
- [ros_protobuf: using protobuf in ROS1](https://github.com/Karsten1987/ros_protobuf/)

在 ROS 2 中使用 Protobuf：
- [eclipse-ecal: rosidl_typesupport_protobuf](https://github.com/eclipse-ecal/rosidl_typesupport_protobuf)（以及各大社区 fork）
- [google: ros-central-registry: the protobuf cpp example of bazel ros2](https://github.com/intrinsic-opensource/ros-central-registry/blob/main/examples)
- [boston dynamics: proto2ros: proto-rosidl(msg) conversion](https://github.com/rai-opensource/proto2ros)

### protoros2 的核心特性 (What protoros2 does)

本项目为 ROS 2 提供了对 Protobuf 的原生全链路支持，**无需对 ros2core（rclcpp, rcl, rmw）进行任何源码修改**。

- **无妥协的 Protobuf 原生体验 (Uncompromised Protobuf Usage)**：以 `.proto` 为统一消息定义，以 `.pb.h` 为 C++ 结构体；通过 RMW 发布 Protobuf 结构体，底层通过中间件直接传输 Protobuf 二进制字节流；支持用 `ros2cli` 查看、`rosbag2` 原生录制落盘、`mcap-cli` 离线读取，全程运行在 Protobuf serdes 之上，并保持与使用标准 rosidl 的存量 ROS 2 节点系统的无缝互通。
- **性能突破 (Performance Improvement)**：基于 `rclcpp::SerializedMessage` 直接向中间件传输序列化后的 Protobuf 消息，彻底规避了传统 rclcpp 层高频触发的 4 次 TypeAdaptation 冗余内存拷贝与转换开销。
- **共享内存极速旁路 (Performance Bonus)**：提供基于 Iceoryx2 共享内存的旁路 pubsub 机制（`FlatChannel`），直接将 Protobuf 序列化至借用内存块（loaned buffer，无额外帧头封装），达成近乎零拷贝（near-zero-copy）的数据传输（仅 1 次序列化和 1 次反序列化）。
- **企业级开发门面 (Enterprise-Grade Experience)**：提供统一的 rclcpp 节点基类 `EnterpriseNode`，开箱即用支持上述所有特性。

---

## 架构设计 (Design)

```
===================== Third-Party Open-Source Foundations (Zero-Intrusive / Read-Only) =====================
+---------------------------------------------+       +----------------------------------------------------+
|                  proto2ros                  |       |            rosidl_typesupport_protobuf             |
|  (AST Parser: .proto -> synthetic .msg IDL) |       |  (TypeSupport & TypeAdapter C++ Handle Generator)  |
+----------------------+----------------------+       +-------------------------+--------------------------+
                       │                                                        │
                       └───────────────────────────┬────────────────────────────┘
                                                   ▼
========================= Proprietary Core Architecture (@packages/protoros2/) =========================
                               +---------------------------------------+
                               |     protoros2 (Wrapper & Core)        |
                               |                                       |
                               |  * Tri-State Orchestration Engine     |
                               |    (Supports ucA, ucB.1, and ucB.2)   |
                               |  * enterprise_node                    |
                               |    (Flat/Proto Channel & Executor API)|
                               +-------------------+-------------------+
                                                   │
                   ┌───────────────────────────────┴───────────────────────────────┐
                   ▼                                                               ▼
+--------------------------------------+                          +--------------------------------------+
|       RMW & Transport Layer          |                          |      MLOps & Rosbag2 Ecosystem       |
|                                      |                          |                                      |
|  * rmw_ecal_proto_cpp                |                          |  * rosbag2_cpp_protobuf_converter    |
|    (Direct Proto Serdes Engine)      |                          |    (-f protobuf Plugin Adapter)      |
|                                      |                          |  * MCAP / Rosbag Readers             |
|                                      |                          |    (mcap_proto & rosbag2_reader)     |
+--------------------------------------+                          +--------------------------------------+
```

---

## 演示与实操指南 (Demo)

### 环境依赖导入 (Setup)

导入外部第三方依赖仓库（请使用项目专用 fork 分支，勿直接使用上游官方源）：

```sh
cd ./3rdparty
vcs import < ../ros2-core.repos
```

---

### Use Case A: 以 rosidl (.msg) 为单一事实源 (SSOT)

#### ucA.1: 运行在仅支持 CDR 的标准 RMW 上 (如 rmw_fastrtps_cpp)

```sh
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
# 1. rclcpp 节点通信
ros2 run protoros2_example example_proto_publisher
ros2 run protoros2_example example_proto_subscriber
# 2. rclpy 命令行验证
ros2 topic list
ros2 topic echo /proto_msg_topic
# 3. rosbag2 录包（使用默认 CDR 序列化插件，不执行二次转换）
ros2 bag record -o proto_msg_rmw_default --topics /proto_msg_topic
ros2 bag info ./proto_msg_rmw_default/
# 4. BagReader 离线解析示例
ros2 run protoros2_example rosbag2_reader.py ./proto_msg_rmw_default/
ros2 run protoros2_example rosbags_reader.py ./proto_msg_rmw_default/
# 5. MLOps / MCAP 读取
mcap info ./proto_msg_rmw_default/0_*.mcap
ros2 run protoros2_example mcap_ros2_reader.py ./proto_msg_rmw_default/0_*.mcap
ros2 run protoros2_example mcap_proto_reader.py ./proto_msg_rmw_default/0_*.mcap
```

#### ucA.2: 运行在分别独立支持 CDR 和 Protobuf 的 RMW 上

导入 rmw_ecal 专用依赖仓库（请使用本仓库对应的 fork 分支）：

```sh
cd ./3rdparty
vcs import < ../ecal.repos
```

使用仅支持 CDR 传输的 `rmw_ecal_dynamic_cpp`（表现行为与标准 `rmw_fastrtps_dynamic_cpp` 对齐）：

```sh
export RMW_IMPLEMENTATION=rmw_ecal_dynamic_cpp
# rclcpp 通信
ros2 run protoros2_example example_proto_publisher
ros2 run protoros2_example example_proto_subscriber
# rclpy 工具链
ros2 topic list
ros2 topic echo /proto_msg_topic
# rosbag2 录制
ros2 bag record -o proto_msg_rmw_ecal_dynamic --topics /proto_msg_topic
ros2 bag info ./proto_msg_rmw_ecal_dynamic/
# 离线解析
ros2 run protoros2_example rosbag2_reader.py ./proto_msg_rmw_ecal_dynamic/
ros2 run protoros2_example rosbags_reader.py ./proto_msg_rmw_ecal_dynamic/
# MLOps 离线工具
mcap info ./proto_msg_rmw_ecal_dynamic/0_*.mcap
ros2 run protoros2_example mcap_ros2_reader.py ./proto_msg_rmw_ecal_dynamic/0_*.mcap
ros2 run protoros2_example mcap_proto_reader.py ./proto_msg_rmw_ecal_dynamic/0_*.mcap
```

使用原生支持 Protobuf 的 `rmw_ecal_proto_cpp`（RMW 传输层直接采用 Protobuf serdes）：

```sh
export RMW_IMPLEMENTATION=rmw_ecal_proto_cpp
# rclcpp 原生 Protobuf 传输
ros2 run protoros2_example example_proto_publisher
ros2 run protoros2_example example_proto_subscriber
# rclpy 工具链
ros2 topic list
ros2 topic echo /proto_msg_topic
# rosbag2 原生 Protobuf 格式录制
ros2 bag record -o proto_msg_rmw_ecal_proto --topics /proto_msg_topic -f protobuf
ros2 bag info ./proto_msg_rmw_ecal_proto/
# 离线解析
ros2 run protoros2_example rosbag2_reader.py ./proto_msg_rmw_ecal_proto/
ros2 run protoros2_example rosbags_reader.py ./proto_msg_rmw_ecal_proto/ # 当前未支持纯 CDR 解析器读取 proto
# MLOps 纯 Python 解析
mcap info ./proto_msg_rmw_ecal_proto/0_*.mcap
ros2 run protoros2_example mcap_ros2_reader.py ./proto_msg_rmw_ecal_proto/0_*.mcap
ros2 run protoros2_example mcap_proto_reader.py ./proto_msg_rmw_ecal_proto/0_*.mcap
```

#### ucA.3: 运行在同时混合支持 CDR 与 Protobuf 的场景下

适用场景：同一系统内存在尚未生成 Protobuf TypeSupport 的存量第三方标准 `.msg` 接口。

> **注意**：在 Fallback 降级模式下，数据包中的 CDR 序列化数据虽然可能被标记为 protobuf 格式标识，但其实际承载的物理载荷仍为标准 CDR。

##### ucA.3.1: 运行基于 rmw_ecal_proto_cpp 的动态自动降级

`rmw_ecal_proto_cpp` 现已原生支持同时处理 CDR 与 Protobuf 流量（当目标 topic 缺乏 Protobuf TypeSupport 时，自动无缝降级回退至 CDR）。

```sh
export RMW_IMPLEMENTATION=rmw_ecal_proto_cpp
# 1. 仅具备 CDR TypeSupport 的传统预编译节点（自动降级为 CDR）
# example_interfaces/msg/String.msg 未编译 rosidl_typesupport_protobuf，缺乏 Protobuf 支持
ros2 run demo_nodes_cpp talker
ros2 run demo_nodes_cpp listener
# 2. 具备 Protobuf TypeSupport 的原生节点（走 Protobuf 原生快速路径）
ros2 run protoros2_example example_proto_publisher
ros2 run protoros2_example example_proto_subscriber
# 3. rclpy 工具链同时侦测不同格式话题
ros2 topic list
ros2 topic echo /chatter
ros2 topic echo /proto_msg_topic
# 4. rosbag2 混合并发录制（同时录入 CDR 话题与 Protobuf 话题）
ros2 bag record -o proto_msg_rmw_ecal_both --topics /chatter /proto_msg_topic
ros2 bag info ./proto_msg_rmw_ecal_both/
# 5. 统一数据读取
ros2 run protoros2_example rosbag2_reader.py ./proto_msg_rmw_ecal_both/
# 6. MLOps 读取
mcap info ./proto_msg_rmw_ecal_both/0_*.mcap
ros2 run protoros2_example mcap_ros2_reader.py ./proto_msg_rmw_ecal_both/0_*.mcap
ros2 run protoros2_example mcap_proto_reader.py ./proto_msg_rmw_ecal_both/0_*.mcap
```

---

### Use Case B: 以 Protobuf (.proto) 为单一事实源 (SSOT)

导入 proto2ros 依赖仓库（请使用项目专用 fork 分支）：

```sh
cd ./3rdparty
vcs import < ../rai.repos
```

#### ucB.1: proto2ros 与 rosidl_typesupport_protobuf 共存模式

启用对应 CMake 构建选项（默认为 `OFF`）：

```cmake
option(PROTO_SSOT "Use Proto file coexisting with rosidl_typesupport_protobuf (ucB.1)" OFF)
```

架构数据流向如下：

```
           [ Left Branch: Use Case A ]                 [ Right Branch: Use Case B ]
            ROS 2 原生数据流 (.msg SSOT)                 AI / 机器人原生流 (.proto SSOT)
                         │                                           │
                         ▼                                           ▼
               rosidl_generate_interfaces                proto2ros (合成镜像 .msg IDL)
                         │                                           │
                         ▼                                           │
          [ Translator: .msg -> 合成 .proto ]                        │ (Direct SSOT .proto)
                         │                                           │
                         └─────────────────────┬─────────────────────┘
                                               ▼
                             [ Shared Backbone: TypeSupport 统一引擎 ]
                     rosidl_typesupport_protobuf (统一生成引擎):
                 生成 C++ TypeSupport 句柄、TypeAdapter 与 Proto Serdes 代码
                                               │
                                               ▼
                       [ rmw_ecal_proto_cpp: 原生零冗余 Serdes 快速通道 ]
```

> **注意**：在此模式下，`.proto` 的 package 命名必须严格遵守命名规则：`<ros2_package_name>.<folder_name>.pb`。
> 上述任何 RMW 均可使用，此处以 `rmw_ecal_proto_cpp` 为例：

```sh
export RMW_IMPLEMENTATION=rmw_ecal_proto_cpp
# rclcpp
ros2 run protoros2_example example_proto_publisher
ros2 run protoros2_example example_proto_subscriber
# rclpy
ros2 topic list
ros2 topic echo /proto_msg_topic
# rosbag2 录制
ros2 bag record -o proto_msg_rmw_ecal_proto --topics /proto_msg_topic -f protobuf
ros2 bag info ./proto_msg_rmw_ecal_proto/
# 离线读取
ros2 run protoros2_example rosbag2_reader.py ./proto_msg_rmw_ecal_proto/
ros2 run protoros2_example rosbags_reader.py ./proto_msg_rmw_ecal_proto/ # 暂不支持
# MLOps 工具
mcap info ./proto_msg_rmw_ecal_proto/0_*.mcap
ros2 run protoros2_example mcap_ros2_reader.py ./proto_msg_rmw_ecal_proto/0_*.mcap
ros2 run protoros2_example mcap_proto_reader.py ./proto_msg_rmw_ecal_proto/0_*.mcap
```

#### ucB.2: 完全剥离 rosidl 生成链路，仅以 proto 作为绝对 SSOT

启用 CMake 选项（默认为 `OFF`）：

```cmake
option(PROTO_SSOT_ONLY "Use Proto file as the ONLY SSOT directly, bypassing rosidl_adapter_proto (ucB.2)" OFF)
```

架构数据流向如下：

```
                                 [ Single Source of Truth (.proto SSOT) ]
                                                    │
                 ┌──────────────────────────────────┴──────────────────────────────────┐
                 ▼                                                                     ▼
  [ 镜像 IDL 分支: 标准 ROS 2 兼容生态 ]                                  [ 原生 Proto 分支: 双引擎直调 Serdes ]
                 │                                                                     │
                 ▼                                                                     ▼
  proto2ros (生成镜像 .msg)                                            引擎 1: 官方原生 protoc --cpp_out
                 │                                                   (生成原生 C++ .pb.h/.pb.cc 与 Python _pb2.py)
                 ▼                                                                     │
     rosidl_generate_interfaces (已深度过滤!)                                           ▼
  ❌ 彻底阻断 .msg -> .proto 二次逆向合成                                引擎 2: rosidl_typesupport_protobuf_cpp
  (剥离 rosidl_adapter_proto 与内部冗余生成器)                          (直接作用于原始 SSOT .proto 源文件)
                 │                                                                     │
                 ▼                                                                     ▼
  生成 ROS 2 标准 C++ / Python 结构体                                 生成 TypeSupport 句柄、TypeAdapter 与 Serdes
  (用于 Introspection 反射、fastrtps 兼容、类型桥接)                    (原生零开销直接绑定至原始 .proto 类)
                 │                                                                     │
                 └──────────────────────────────────┬──────────────────────────────────┘
                                                    ▼
                                    [ Shared RMW & Application Layer ]
                             rmw_ecal_proto_cpp / rclcpp / TypeAdapter Zero-Copy Bridge
```

> **注意**：在此模式下，`.proto` 的 package 命名同样必须严格遵守：`<ros2_package_name>.<folder_name>.pb`。

```sh
export RMW_IMPLEMENTATION=rmw_ecal_proto_cpp
# rclcpp
ros2 run protoros2_example example_proto_publisher
ros2 run protoros2_example example_proto_subscriber
# rclpy
ros2 topic list
ros2 topic echo /proto_msg_topic
# rosbag2 录制
ros2 bag record -o proto_msg_rmw_ecal_proto --topics /proto_msg_topic -f protobuf
ros2 bag info ./proto_msg_rmw_ecal_proto/
# 离线读取
ros2 run protoros2_example rosbag2_reader.py ./proto_msg_rmw_ecal_proto/
ros2 run protoros2_example rosbags_reader.py ./proto_msg_rmw_ecal_proto/ # 暂不支持
# MLOps 工具
mcap info ./proto_msg_rmw_ecal_proto/0_*.mcap
ros2 run protoros2_example mcap_ros2_reader.py ./proto_msg_rmw_ecal_proto/0_*.mcap
ros2 run protoros2_example mcap_proto_reader.py ./proto_msg_rmw_ecal_proto/0_*.mcap
```

---

### Use Case C: 使用 EnterpriseNode 实施通信加速

#### ucC.1: 使用 proto_publisher 与 proto_subscriber

相较标准 TypeAdapter 通道，直接绕过 2 次 rclcpp 内部的中间格式拷贝转换：

```sh
export RMW_IMPLEMENTATION=rmw_ecal_proto_cpp
ros2 run protoros2_example enterprise_proto_publisher
ros2 run protoros2_example enterprise_proto_subscriber
```

#### ucC.2: 使用 flat_publisher 与 flat_subscriber

基于 Iceoryx2 共享内存后端构建无锁零拷贝极速通道：

```sh
export RMW_IMPLEMENTATION=rmw_ecal_proto_cpp
ros2 run protoros2_example enterprise_flat_publisher
ros2 run protoros2_example enterprise_flat_subscriber
```

#### ucC.3: 图像传输基准压测 (Image Transport Benchmark)

```sh
export RMW_IMPLEMENTATION=rmw_ecal_proto_cpp
# 启动订阅端接收压测 (支持通过 -p channel_type:=proto 进行 A/B 性能对比，发布端与订阅端通道类型需保持一致)
ros2 run protoros2_example enterprise_image_listener --ros-args -p decode_and_verify:=false
```

```sh
export RMW_IMPLEMENTATION=rmw_ecal_proto_cpp
# 1080p (3.3 MB JPEG) 压测
ros2 run protoros2_example enterprise_image_talker --ros-args -p image_path:=1920_1080.jpg -p frequency:=10.0

# 4K (5.0 MB JPEG) 压测
ros2 run protoros2_example enterprise_image_talker --ros-args -p image_path:=6000_4000.jpg -p frequency:=5.0
```

#### ucC.4: Protobuf 反序列化性能压测 (Protobuf Parsing Benchmark)

```sh
export RMW_IMPLEMENTATION=rmw_ecal_proto_cpp
# 接收端输出滑动窗口统计信息并在退出时打印 BENCH SUMMARY (可通过 -p zero_copy_parse:=false 进行 A/B 对比测试)
# (发布端与接收端需使用相同的 channel_type)
ros2 run protoros2_example enterprise_bytes_listener
```

```sh
export RMW_IMPLEMENTATION=rmw_ecal_proto_cpp
# 模式 1: 浅层大包 (3 MB x 1 @ 10 Hz)
ros2 run protoros2_example enterprise_bytes_talker --ros-args -p payload_size:=3145728 -p chunk_count:=1 -p frequency:=10.0

# 模式 2: 多 repeated 嵌套中等包 (16 KB x 64 @ 30 Hz)
ros2 run protoros2_example enterprise_bytes_talker --ros-args -p payload_size:=1048576 -p chunk_count:=64 -p frequency:=30.0

# 模式 3: 高频极小包 (2 KB x 1 @ 200 Hz)
ros2 run protoros2_example enterprise_bytes_talker --ros-args -p payload_size:=2048 -p chunk_count:=1 -p frequency:=200.0
```

---

### Use Case D: 以 EnterpriseNode 深度替代默认 rclcpp::Node

`enterprise_proto` 通过内部持有的 `rclcpp::SubscriptionBase` 代理，原生支持标准 ROS 2 调度范式（包括 WaitSet、Component 组件、CallbackGroup 回调组）；`enterprise_flat` 则提供自定义 Waitable 适配器，将 Iceoryx2 原生调度无缝接入 ROS 2 Executor 执行器体系。

#### ucD.1: CallbackGroup 回调组订阅者

```sh
export RMW_IMPLEMENTATION=rmw_ecal_proto_cpp

ros2 run protoros2_example enterprise_flat_publisher
ros2 run protoros2_example enterprise_flat_callback_group_subscriber
```

#### ucD.2: WaitSet 订阅者

```sh
export RMW_IMPLEMENTATION=rmw_ecal_proto_cpp

ros2 run protoros2_example enterprise_flat_publisher
ros2 run protoros2_example enterprise_flat_waitset_subscriber
```

#### ucD.3: 纯 Pull / Polling 轮询模式订阅者

```sh
export RMW_IMPLEMENTATION=rmw_ecal_proto_cpp

# 1. Flat Channel 轮询订阅者 (基于 Iceoryx2 Waitable 适配)
ros2 run protoros2_example enterprise_flat_publisher
ros2 run protoros2_example enterprise_flat_polling_subscriber

# 2. Proto Channel 轮询订阅者 (基于互斥 CallbackGroup 模式)
ros2 run protoros2_example enterprise_proto_publisher
ros2 run protoros2_example enterprise_proto_polling_subscriber
```

#### ucD.4: 节点进程内通信零拷贝 (Intra-Process Zero-Copy)

`enterprise_proto` 原生支持基于 rclcpp TypeAdapter 的进程内直接传针零拷贝通信：

```sh
export RMW_IMPLEMENTATION=rmw_ecal_proto_cpp

ros2 run protoros2_example enterprise_proto_intra_demo
```

---

## 社区讨论 (Discussion)

- [ROS Discourse 社区讨论帖](https://discourse.openrobotics.org/t/announcing-protoros2-use-protobuf-in-ros2-without-compromise/57152)

---

## 致谢 (Acknowledgement)

- 本项目代码全流程深度践行 Taste-Driven 敏捷研发，由 [agy-cli](https://antigravity.google/docs/cli/overview) 辅助加速完成。
- 项目命名灵感源自知名开源项目 [flatros2](https://github.com/Ekumen-OS/flatros2)。

---

## 规划与任务路线 (TODO)

### 1. Protobuf Serdes 底层性能优化

#### 反序列化与解析接口 (Protobuf Parsing API)
*基线*：Ubuntu 26.04 系统预装 Protobuf 3.21.12 下的标准 `ParseFromString`。

- [x] `ParseFromArray`
- [ ] `ParseFromZeroCopyStream` + `Aliasing`（零拷贝内存别名解析，Protobuf 3.21.12 暂未完整开放）
- [ ] `ctype=CORD` 支持（需 Protobuf v23.0+）
- [ ] `ctype=string_view` 支持（需 Protobuf Edition 2023 / v26.0+）

#### 内存分配优化 (Protobuf Memory Alloc API)
*基线*：基于 `thread_local` 局部缓存。

- [ ] `Arena` 内存池分配
- [ ] `placement-new Arena` 共享内存原地反序列化

### 2. protoros2 核心组件演进

*基线*：`rmw_ecal_proto_cpp` + eCAL (v5.13 预编译依赖，基于 pb 3.21.7)。

- [x] `enterprise_flat` 适配 Iceoryx2 共享内存后端
- [ ] `rmw_zenoh_proto_cpp` 传输层支持

### 3. 系统稳定性与工程化

- [ ] 增加系统级端到端测试 (`system_test`)
- [ ] 接入自动化持续集成流水线 (`CI`)

---

## 已知问题 (Known Issues / Bugs)

针对 `rmw_ecal` / `ecal`：

- [ ] `ros2 run example_proto_publisher/subscriber` 退出阶段可能偶现警告：`[Service Server] Service shutting down: Operation aborted.`
- [ ] `ros2 run example_proto_publisher/subscriber` 退出清理时可能偶现指针异常：`free(): invalid pointer [ros2run]: Aborted`
