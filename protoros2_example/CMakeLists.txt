cmake_minimum_required(VERSION 3.20)
project(protoros2_example)

# Default to C++17
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(rosidl_typesupport_protobuf_cpp REQUIRED)

find_package(rclpy REQUIRED)
find_package(rosbag2_py REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  # https://github.com/intrinsic-opensource/ros-central-registry/blob/lyrical/2026-06-08/examples/msg/ExampleMessage.msg
  "msg/ExampleMessage.msg"
  DEPENDENCIES
)
rosidl_get_typesupport_target(cpp_typesupport_target "${PROJECT_NAME}" "rosidl_typesupport_cpp")
rosidl_get_typesupport_target(protobuf_cpp_typesupport_target "${PROJECT_NAME}" "rosidl_typesupport_protobuf_cpp")

add_executable(example_proto_publisher ./src/example_proto_publisher.cpp)
target_link_libraries(example_proto_publisher PRIVATE rclcpp::rclcpp std_msgs::std_msgs ${cpp_typesupport_target} ${protobuf_cpp_typesupport_target} ${PROJECT_NAME}__rosidl_generator_cpp)

add_executable(example_proto_subscriber ./src/example_proto_subscriber.cpp)
target_link_libraries(example_proto_subscriber PRIVATE rclcpp::rclcpp std_msgs::std_msgs ${cpp_typesupport_target} ${protobuf_cpp_typesupport_target} ${PROJECT_NAME}__rosidl_generator_cpp)

install(TARGETS
  example_proto_publisher
  example_proto_subscriber
  DESTINATION lib/${PROJECT_NAME}
)

install(PROGRAMS
  scripts/rosbag2_reader.py
  scripts/rosbags_reader.py
  DESTINATION lib/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_export_dependencies(rosidl_default_runtime)
ament_package()
