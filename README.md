# protoros2

the name of this repo is inspired by [flatros2](https://github.com/Ekumen-OS/flatros2)

## overview

### motivation

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
# badreader example
ros2 run protoros2_example rosbag2_reader.py ./proto_msg_rmw_default/
ros2 run protoros2_example rosbags_reader.py ./proto_msg_rmw_default/
```


## acknowledgement

## todo
