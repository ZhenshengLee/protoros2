# protoros2

for Chinese readers, please refer to [简体中文](README_zh.md)

## introduction

zero-intrusive protobuf support for ros2.

### motivation

- in 2017, [the developer of apollo suggest using protobuf in ros1(ros_comm) ](https://github.com/ros/ros_comm/issues/1085)
- in 2013, the [ros2 design artical: ROS2 Message Research](https://design.ros2.org/articles/serialization.html) suggest that Serialization should be optional, and should use existing library
- the arch of ros2 derives from [ros2 design artical: ROS on DDS](https://design.ros2.org/articles/ros_on_dds.html), but there is a clear trend that serialization agnostic middleware like iceoryx, ecal, and zenoh are becoming popular.
- the report [eprosima: Apache Thrift vs Protocol Buffers vs Fast Buffers](https://www.eprosima.com/developer-resources/performance/apache-thrift-vs-protocol-buffers-vs-fast-buffers) shows that the performance of fastcdr better than protobuf(v2.5) in pubsub benchmark, but new high-performance features have been introduced, including [protobuf: arenas](https://protobuf.dev/reference/cpp/arenas/), [protobuf: String View APIs](https://protobuf.dev/reference/cpp/string-view/), [protobuf: ctype=CORD](https://protobuf.dev/news/2023-04-11/), [protobuf: New RepeatedPtrField Layout](https://protobuf.dev/news/2025-09-19/#cpp-repeatedptrfield-layout)
- protobuf is backed by Google and widely used across the AI industry.
- protobuf has traditional advantages such as ease of use, version compatibility, multi-language support, edge/cloud/mcu compatibility.
- many robotics/sdv middlewares are using protobuf, like w----e, h-----n, etc.

### related work

use protobuf in ROS1:

- [ros_protobuf: using protobuf in ROS1](https://github.com/Karsten1987/ros_protobuf/)

use protobuf in ROS2:

- [eclipse-ecal: rosidl_typesupport_protobuf](https://github.com/eclipse-ecal/rosidl_typesupport_protobuf) (and many forks)
- [google: ros-central-registry: the protobuf cpp example of bazel ros2](https://github.com/intrinsic-opensource/ros-central-registry/blob/main/examples)
- [boston dynamics: proto2ros: proto-rosidl(msg) conversion](https://github.com/rai-opensource/proto2ros)

### what protoros2 do

this repo provides protobuf support to ros2, without any modification to ros2core(rclcpp, rcl, rmw).

- uncompromise protobuf usage in ros2: use protobuf as msg definition, pb.h as struct, publish the pb struct through rmw, transport pb binary through mw, use ros2cli to check, use rosbag2 to record, and use mcap-cli to read bag, all with protobuf serdes, remain interoperability with prebuilt ros2 system that use rosidl.
- performance improvement: use rclcpp::SerializedMessage to direct publish the protobuf serialized message to mw, to avoid 4x typeadaption conversions in rclcpp layer.
- performance bonus: a bypass pubsub mechanism based on iceoryx2 shared memory, serializing protobuf directly into the loaned buffer (no framing), to get near-zero-copy data transport(1x ser and 1x deser).
- enterprise grade experience: an unified rclcpp node interface enterprise_node provides features above.

## design

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

## demo

### setup

import external code (use the fork rather than the official repo)

```sh
cd ./3rdparty
vcs import < ../ros2-core.repos
```

### usecaseA: use rosidl(msg) as the SSOT(single source of truth)

#### ucA.1: run with rmw that support cdr only

```sh
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
# rclcpp
ros2 run protoros2_example example_proto_publisher
ros2 run protoros2_example example_proto_subscriber
# rclpy
ros2 topic list
ros2 topic echo /proto_msg_topic
# rosbag2 using default serialization plugin which is cdr, will not convert
ros2 bag record -o proto_msg_rmw_default --topics /proto_msg_topic
ros2 bag info ./proto_msg_rmw_default/
# bagreader example
ros2 run protoros2_example rosbag2_reader.py ./proto_msg_rmw_default/
ros2 run protoros2_example rosbags_reader.py ./proto_msg_rmw_default/
# mlops
mcap info ./proto_msg_rmw_default/0_*.mcap
ros2 run protoros2_example mcap_ros2_reader.py ./proto_msg_rmw_default/0_*.mcap
ros2 run protoros2_example mcap_proto_reader.py ./proto_msg_rmw_default/0_*.mcap
```

#### ucA.2: run with rmw that support cdr and protobuf separately

need to import rmw_ecal (use the fork rather than the official repo)

```sh
cd ./3rdparty
vcs import < ../ecal.repos
```

use rmw_ecal_dynamic_cpp only support cdr, will be like with rmw_fastrtps_dynamic_cpp

```sh
export RMW_IMPLEMENTATION=rmw_ecal_dynamic_cpp
# rclcpp
ros2 run protoros2_example example_proto_publisher
ros2 run protoros2_example example_proto_subscriber
# rclpy
ros2 topic list
ros2 topic echo /proto_msg_topic
# rosbag2
ros2 bag record -o proto_msg_rmw_ecal_dynamic --topics /proto_msg_topic
ros2 bag info ./proto_msg_rmw_ecal_dynamic/
# bagreader
ros2 run protoros2_example rosbag2_reader.py ./proto_msg_rmw_ecal_dynamic/
ros2 run protoros2_example rosbags_reader.py ./proto_msg_rmw_ecal_dynamic/
# mlops
mcap info ./proto_msg_rmw_ecal_dynamic/0_*.mcap
ros2 run protoros2_example mcap_ros2_reader.py ./proto_msg_rmw_ecal_dynamic/0_*.mcap
ros2 run protoros2_example mcap_proto_reader.py ./proto_msg_rmw_ecal_dynamic/0_*.mcap
```

use rmw_ecal_proto_cpp only support protobuf, will use protobuf serdes in rmw layer

```sh
export RMW_IMPLEMENTATION=rmw_ecal_proto_cpp
# rclcpp
ros2 run protoros2_example example_proto_publisher
ros2 run protoros2_example example_proto_subscriber
# rclpy
ros2 topic list
ros2 topic echo /proto_msg_topic
# rosbag2
ros2 bag record -o proto_msg_rmw_ecal_proto --topics /proto_msg_topic -f protobuf
ros2 bag info ./proto_msg_rmw_ecal_proto/
# bagreader
ros2 run protoros2_example rosbag2_reader.py ./proto_msg_rmw_ecal_proto/
ros2 run protoros2_example rosbags_reader.py ./proto_msg_rmw_ecal_proto/ # not supported currently
# mlops
mcap info ./proto_msg_rmw_ecal_proto/0_*.mcap
ros2 run protoros2_example mcap_ros2_reader.py ./proto_msg_rmw_ecal_proto/0_*.mcap
ros2 run protoros2_example mcap_proto_reader.py ./proto_msg_rmw_ecal_proto/0_*.mcap
```

#### ucA.3: run with rmw that support cdr and protobuf simultaneously

that's the case when: the rosidl(msg) that is not compiled with protobuf typesupport.

note: when in fallback mode, the cdr serdes data in the bag will be flagged as protobuf, but the actual data is cdr.

##### ucA.3.1: run with rmw_ecal_proto_cpp

`rmw_ecal_proto_cpp` now supports cdr and protobuf simultaneously (with automatic fallback to cdr when protobuf typesupport is not available).

```sh
export RMW_IMPLEMENTATION=rmw_ecal_proto_cpp
# 1. rclcpp with prebuilt binary with cdr typesupport only (fallback to cdr)
# example_interface/msg/String.msg has not been compiled with rosidl_typesupport_protobuf, thus dont have protobuf typesupport
ros2 run demo_nodes_cpp talker
ros2 run demo_nodes_cpp listener
# 2. rclcpp with protobuf typesupport
ros2 run protoros2_example example_proto_publisher
ros2 run protoros2_example example_proto_subscriber
# 3. rclpy
ros2 topic list
ros2 topic echo /chatter
ros2 topic echo /proto_msg_topic
# 4. rosbag2 record & info (simultaneous cdr + protobuf topics)
ros2 bag record -o proto_msg_rmw_ecal_both --topics /chatter /proto_msg_topic
ros2 bag info ./proto_msg_rmw_ecal_both/
# 5. bagreader
ros2 run protoros2_example rosbag2_reader.py ./proto_msg_rmw_ecal_both/
# 6. mlops
mcap info ./proto_msg_rmw_ecal_both/0_*.mcap
ros2 run protoros2_example mcap_ros2_reader.py ./proto_msg_rmw_ecal_both/0_*.mcap
ros2 run protoros2_example mcap_proto_reader.py ./proto_msg_rmw_ecal_both/0_*.mcap
```

### usecaseB: use proto as the SSOT

need to import proto2ros (use the fork rather than the official repo)

```sh
cd ./3rdparty
vcs import < ../rai.repos
```

#### ucB.1: proto2ros coexists with rosidl_typesupport_protobuf

turn on the cmake option(default OFF)

```cmake
option(PROTO_SSOT "Use Proto file coexisting with rosidl_typesupport_protobuf (ucB.1)" OFF)
```

the arch is as follows:

```
           [ Left Branch: Use Case A ]                 [ Right Branch: Use Case B ]
           ROS 2 Native Stream (.msg SSOT)             AI / Robotics Stream (.proto SSOT)
                         │                                           │
                         ▼                                           ▼
               rosidl_generate_interfaces                proto2ros (Generate Mirror IDL)
                         │                                           │
                         ▼                                           │
          [ Translator: .msg -> synthetic .proto ]                   │ (Direct SSOT .proto)
                         │                                           │
                         └─────────────────────┬─────────────────────┘
                                               ▼
                             [ Shared Backbone: TypeSupport Engine ]
                     rosidl_typesupport_protobuf (Unified Generator Engine):
                 Generates C++ TypeSupport Handle, TypeAdapter & Proto Serdes
                                               │
                                               ▼
                       [ rmw_ecal_proto_cpp: Direct Zero-Overhead Serdes ]
```

note: the proto definition must follow the rule: <ros2_package_name>.<folder_name>.pb

note: use any of rmw above should be ok, take rmw_ecal_proto_cpp for example

```sh
export RMW_IMPLEMENTATION=rmw_ecal_proto_cpp
# rclcpp
ros2 run protoros2_example example_proto_publisher
ros2 run protoros2_example example_proto_subscriber
# rclpy
ros2 topic list
ros2 topic echo /proto_msg_topic
# rosbag2
ros2 bag record -o proto_msg_rmw_ecal_proto --topics /proto_msg_topic -f protobuf
ros2 bag info ./proto_msg_rmw_ecal_proto/
# bagreader
ros2 run protoros2_example rosbag2_reader.py ./proto_msg_rmw_ecal_proto/
ros2 run protoros2_example rosbags_reader.py ./proto_msg_rmw_ecal_proto/ # not supported currently
# mlops
mcap info ./proto_msg_rmw_ecal_proto/0_*.mcap
ros2 run protoros2_example mcap_ros2_reader.py ./proto_msg_rmw_ecal_proto/0_*.mcap
ros2 run protoros2_example mcap_proto_reader.py ./proto_msg_rmw_ecal_proto/0_*.mcap
```

#### ucB.2: remove rosidl(msg) support and only use proto as the SSOT

turn on the cmake option(default OFF)

```cmake
option(PROTO_SSOT_ONLY "Use Proto file as the ONLY SSOT directly, bypassing rosidl_adapter_proto (ucB.2)" OFF)
```

the arch is as follows:

```
                                 [ Single Source of Truth (.proto SSOT) ]
                                                    │
                 ┌──────────────────────────────────┴──────────────────────────────────┐
                 ▼                                                                     ▼
 [ Mirror IDL Branch: Standard ROS 2 ]                              [ Direct Proto Branch: Dual-Engine Serdes ]
                 │                                                                     │
                 ▼                                                                     ▼
 proto2ros (Generate Mirror .msg)                                     Engine 1: Direct protoc --cpp_out
                 │                                                   (Native C++ .pb.h/.pb.cc & Python _pb2.py)
                 ▼                                                                     │
    rosidl_generate_interfaces (Filtered!)                                             ▼
  ❌ No .msg -> .proto synthetic conversion                           Engine 2: rosidl_typesupport_protobuf_cpp
  (rosidl_adapter_proto & protobuf ts stripped)                        (Custom generator invoked on .proto SSOT)
                 │                                                                     │
                 ▼                                                                     ▼
  Generates ROS 2 Standard C++ / Python Structs                 Generates TypeSupport Handle, TypeAdapter & Serdes
  (Introspection, fastrtps, conversions bridge)                  (Direct zero-overhead binding to original .proto)
                 │                                                                     │
                 └──────────────────────────────────┬──────────────────────────────────┘
                                                    ▼
                                    [ Shared RMW & Application Layer ]
                             rmw_ecal_proto_cpp / rclcpp / TypeAdapter Zero-Copy Bridge
```

note: the proto definition must follow the rule: <ros2_package_name>.<folder_name>.pb

note: use any of rmw above should be ok, take rmw_ecal_proto_cpp for example

```sh
export RMW_IMPLEMENTATION=rmw_ecal_proto_cpp
# rclcpp
ros2 run protoros2_example example_proto_publisher
ros2 run protoros2_example example_proto_subscriber
# rclpy
ros2 topic list
ros2 topic echo /proto_msg_topic
# rosbag2
ros2 bag record -o proto_msg_rmw_ecal_proto --topics /proto_msg_topic -f protobuf
ros2 bag info ./proto_msg_rmw_ecal_proto/
# bagreader
ros2 run protoros2_example rosbag2_reader.py ./proto_msg_rmw_ecal_proto/
ros2 run protoros2_example rosbags_reader.py ./proto_msg_rmw_ecal_proto/ # not supported currently
# mlops
mcap info ./proto_msg_rmw_ecal_proto/0_*.mcap
ros2 run protoros2_example mcap_ros2_reader.py ./proto_msg_rmw_ecal_proto/0_*.mcap
ros2 run protoros2_example mcap_proto_reader.py ./proto_msg_rmw_ecal_proto/0_*.mcap
```

### usecaseC: use enterprise node to accelerate the communication

#### ucC.1: use proto_publisher and proto_subscriber

can avoid 2 typeadaption conversion

```sh
export RMW_IMPLEMENTATION=rmw_ecal_proto_cpp
ros2 run protoros2_example enterprise_proto_publisher
ros2 run protoros2_example enterprise_proto_subscriber
```

#### ucC.2: use flat_publisher and flat_subscriber

```sh
export RMW_IMPLEMENTATION=rmw_ecal_proto_cpp
ros2 run protoros2_example enterprise_flat_publisher
ros2 run protoros2_example enterprise_flat_subscriber
```

#### ucC.3 image transport benchmark

```sh
export RMW_IMPLEMENTATION=rmw_ecal_proto_cpp
# A/B: -p channel_type:=proto (publisher and subscriber must use the same channel_type)
ros2 run protoros2_example enterprise_image_listener --ros-args -p decode_and_verify:=false
```

```sh
export RMW_IMPLEMENTATION=rmw_ecal_proto_cpp
# A/B: -p channel_type:=proto (publisher and subscriber must use the same channel_type)
# 1080p (3.3 MB JPEG)
ros2 run protoros2_example enterprise_image_talker --ros-args -p image_path:=1920_1080.jpg -p frequency:=10.0

# 4k(5 MB JPEG)
ros2 run protoros2_example enterprise_image_talker --ros-args -p image_path:=6000_4000.jpg -p frequency:=5.0
```

#### ucC.4 protobuf parsing benchmark

```sh
export RMW_IMPLEMENTATION=rmw_ecal_proto_cpp
# listener prints per-window stats and a BENCH SUMMARY on exit (use -p zero_copy_parse:=false for A/B)
# A/B: -p channel_type:=proto (publisher and subscriber must use the same channel_type)
ros2 run protoros2_example enterprise_bytes_listener
```

```sh
export RMW_IMPLEMENTATION=rmw_ecal_proto_cpp
# A/B: -p channel_type:=proto (publisher and subscriber must use the same channel_type)
# shallow large (3 MB x 1 @ 10 Hz)
ros2 run protoros2_example enterprise_bytes_talker --ros-args -p payload_size:=3145728 -p chunk_count:=1 -p frequency:=10.0

# repeated medium (16 KB x 64 @ 30 Hz)
ros2 run protoros2_example enterprise_bytes_talker --ros-args -p payload_size:=1048576 -p chunk_count:=64 -p frequency:=30.0

# high-rate small (2 KB x 1 @ 200 Hz)
ros2 run protoros2_example enterprise_bytes_talker --ros-args -p payload_size:=2048 -p chunk_count:=1 -p frequency:=200.0
```

### usecaseD: replace default rclcpp::node with enterprise node

enterprise_proto natively supports ROS 2 standard paradigms (WaitSet, Component, CallbackGroup) through its rclcpp::SubscriptionBase proxy. enterprise_flat provides custom waitables to adapt Iceoryx2 to the ROS 2 executor ecosystem.

#### ucD.1: callback group subscriber

```sh
export RMW_IMPLEMENTATION=rmw_ecal_proto_cpp

ros2 run protoros2_example enterprise_flat_publisher
ros2 run protoros2_example enterprise_flat_callback_group_subscriber
```

#### ucD.2: waitset subscriber

```sh
export RMW_IMPLEMENTATION=rmw_ecal_proto_cpp

ros2 run protoros2_example enterprise_flat_publisher
ros2 run protoros2_example enterprise_flat_waitset_subscriber
```

#### ucD.3: polling subscriber

```sh
export RMW_IMPLEMENTATION=rmw_ecal_proto_cpp

# 1. Flat Channel Polling Subscriber (Iceoryx2 Waitable)
ros2 run protoros2_example enterprise_flat_publisher
ros2 run protoros2_example enterprise_flat_polling_subscriber

# 2. Proto Channel Polling Subscriber (Mutually Exclusive CallbackGroup Pattern)
ros2 run protoros2_example enterprise_proto_publisher
ros2 run protoros2_example enterprise_proto_polling_subscriber
```

#### ucD.4: intra-process-comm zerocopy

enterprise_proto natively supports rclcpp's TypeAdapter intra-process zero-copy communication.

```sh
export RMW_IMPLEMENTATION=rmw_ecal_proto_cpp

ros2 run protoros2_example enterprise_proto_intra_demo
```

## discussion

- [discussion on ros discourse](https://discourse.openrobotics.org/t/announcing-protoros2-use-protobuf-in-ros2-without-compromise/57152)

## acknowledgement

the code is in taste driven development by [agy-cli](https://antigravity.google/docs/cli/overview)

the name of the repo is inspired by [flatros2](https://github.com/Ekumen-OS/flatros2)

## todo

### protobuf serdes performance

#### protobuf parsing api

baseline: ParseFromString with ubuntu 26.04 apt package version 3.21.12

- [x] ParseFromArray
- [ ] ParseFromZeroCopyStream + Aliasing (not available in protobuf 3.21.12)
- [ ] ctype=CORD (available in protobuf v23.0)
- [ ] ctype=string_view (available in protobuf edition 2023 v26.0)

#### protobuf memory alloc api

baseline: thread_local cache

- [ ] Arena
- [ ] placement-new Arena

### protoros2 software components

baseline: rmw_ecal_proto_cpp + ecal(v5.13 prebuilt with pb3.21.7)

- [x] enterprise_flat use iceoryx2 backend
- [ ] rmw_zenoh_proto_cpp support

### protoros2 software stability

- [ ] add system_test
- [ ] add ci

## bugs

rmw_ecal/ecal

- [ ] ros2 run example_proto_publisher/subscriber: [Service Server] Service shutting down: Operation aborted.
- [ ] ros2 run example_proto_publisher/subscriber: free(): invalid pointer [ros2run]: Aborted
