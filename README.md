# protoros2

the name of this repo is inspired by [flatros2](https://github.com/Ekumen-OS/flatros2)

## overview

### motivation

there is old and strong demand for protobuf serialization in ros2, see [the developer of apollo suggest using protobuf in ros1(ros_comm) in 2017](https://github.com/ros/ros_comm/issues/1085)

### related work

[rosidl_typesupport_protobuf](https://github.com/eclipse-ecal/rosidl_typesupport_protobuf) (and many forks)

[ros-central-registry: the protobuf cpp example of bazel ros2](https://github.com/intrinsic-opensource/ros-central-registry/blob/main/examples)

[proto2ros: proto-rosidl(msg) conversion](https://github.com/rai-opensource/proto2ros)

### what protoros2 do

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

## design

## setup

import external code (use the fork rather than the official repo)

```sh
cd ./3rdparty
vcs import < ../ros2-core.repos
```

## demo

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

##### ucA.3.2: run with rmw_iceoryx_proto_cpp

need to import rmw_iceoryx

```sh
cd ./3rdparty
vcs import < ../iceoryx.repos
```

```sh
export RMW_IMPLEMENTATION=rmw_icecoryx_cpp
# start iceoryx daemon first
iox-roudi
# 1. rclcpp with protobuf typesupport
ros2 run protoros2_example example_proto_publisher
ros2 run protoros2_example example_proto_subscriber
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
iox-roudi
ros2 run protoros2_example enterprise_flat_publisher
ros2 run protoros2_example enterprise_flat_subscriber
```

#### ucC.3 image transport benchmark

```sh
# start roudi first
iox-roudi &
```

```sh
ros2 run protoros2_example enterprise_image_listener --ros-args -p decode_and_verify:=false
```

```sh
# 1080p (3.3 MB JPEG)
ros2 run protoros2_example enterprise_image_talker --ros-args -p image_path:=1920_1080.jpg -p frequency:=10.0

# 4k(5 MB JPEG)
ros2 run protoros2_example enterprise_image_talker --ros-args -p image_path:=6000_4000.jpg -p frequency:=5.0
```

### usecaseD: replace default rclcpp::node with enterprise node

enterprise_proto natively supports ROS 2 standard paradigms (WaitSet, Component, CallbackGroup) through its rclcpp::SubscriptionBase proxy. enterprise_flat provides custom waitables to adapt Iceoryx to the ROS 2 executor ecosystem.

#### ucD.1: callback group subscriber

```sh
export RMW_IMPLEMENTATION=rmw_ecal_proto_cpp
# start iceoryx daemon first
iox-roudi &

ros2 run protoros2_example enterprise_flat_publisher
ros2 run protoros2_example enterprise_flat_callback_group_subscriber
```

#### ucD.2: waitset subscriber

```sh
export RMW_IMPLEMENTATION=rmw_ecal_proto_cpp
# start iceoryx daemon first
iox-roudi &

ros2 run protoros2_example enterprise_flat_publisher
ros2 run protoros2_example enterprise_flat_waitset_subscriber
```

#### ucD.3: polling subscriber

```sh
export RMW_IMPLEMENTATION=rmw_ecal_proto_cpp
# start iceoryx daemon first
iox-roudi &

# 1. Flat Channel Polling Subscriber (Iceoryx Waitable)
ros2 run protoros2_example enterprise_flat_publisher
ros2 run protoros2_example enterprise_flat_polling_subscriber

# 2. Proto Channel Polling Subscriber (Mutually Exclusive CallbackGroup Pattern)
ros2 run protoros2_example enterprise_proto_publisher
ros2 run protoros2_example enterprise_proto_polling_subscriber
```

## acknowledgement

[agy-cli](https://antigravity.google/docs/cli/overview)

## todo
