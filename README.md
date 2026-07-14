# protoros2

the name of this repo is inspired by [flatros2](https://github.com/Ekumen-OS/flatros2)

## overview

### motivation

there is old and strong demand for protobuf serialization in ros2, see [the developer of apollo suggest using protobuf in ros1(ros_comm) in 2017](https://github.com/ros/ros_comm/issues/1085)

### related work

[the fork of rosidl_typesupport_protobuf for lyrical](https://github.com/PranavDhulipala/rosidl_typesupport_protobuf)

[the fork of rosidl_typesupport_protobuf for ros2 bazel registry](https://github.com/asymingt/rosidl_typesupport_protobuf)

[the protobuf cpp example of ros2 central registry](https://github.com/intrinsic-opensource/ros-central-registry/blob/main/examples)

### what protoros2 do

## design

## setup

import external code (use the fork rather than the official repo)

```sh
cd ./3rdparty
vcs import < ../ros2-core.repos
```

## demo

### usecaseA: use rosidl(msg) as the single source of truth

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
# rosbag2 using protobuf serialization plugin protobuf, will convert cdr to protobuf in runtime
# note: currently the protobuf converter plugin is not available in the official rosbag2
# ros2 bag record -o proto_msg_rmw_default --topics /proto_msg_topic -f protobuf
# bagreader example
ros2 run protoros2_example rosbag2_reader.py ./proto_msg_rmw_default/
ros2 run protoros2_example rosbags_reader.py ./proto_msg_rmw_default/
# mlops
mcap info ./proto_msg_rmw_default/0_*.mcap
ros2 run protoros2_example mcap_ros2_reader.py ./proto_msg_rmw_default/0_*.mcap
ros2 run protoros2_example mcap_proto_reader.py ./proto_msg_rmw_default/0_*.mcap
```

#### ucA.2: run with rmw that support cdr and protobuf separately

need to import rmw_ecal

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

## acknowledgement

[agy-cli](https://antigravity.google/docs/cli/overview)

## todo
