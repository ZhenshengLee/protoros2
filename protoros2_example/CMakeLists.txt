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

option(PROTO_SSOT "Use Proto file coexisting with rosidl_typesupport_protobuf (ucB.1)" OFF)
option(PROTO_SSOT_ONLY "Use Proto file as the ONLY SSOT directly, bypassing rosidl_adapter_proto (ucB.2)" OFF)

if(PROTO_SSOT AND PROTO_SSOT_ONLY)
  message(FATAL_ERROR "PROTO_SSOT (ucB.1) and PROTO_SSOT_ONLY (ucB.2) cannot both be ON simultaneously!")
endif()

if(PROTO_SSOT_ONLY)
  message(STATUS "Using PROTO_SSOT_ONLY (pure .proto SSOT via protoros2 wrapper, ucB.2) for ${PROJECT_NAME}")
  find_package(protoros2 REQUIRED)
  proto2ros_vendor_package(${PROJECT_NAME}
    PROTO_SSOT_ONLY
    PROTOS "msg/ExampleMessage.proto"
  )
  set(CONVERSIONS_LIB ${PROJECT_NAME}_conversions)
elseif(PROTO_SSOT)
  message(STATUS "Using PROTO_SSOT (proto2ros coexisting with rosidl_typesupport_protobuf, ucB.1) for ${PROJECT_NAME}")
  find_package(protoros2 REQUIRED)
  proto2ros_vendor_package(${PROJECT_NAME}
    PROTOS "msg/ExampleMessage.proto"
  )
  set(CONVERSIONS_LIB ${PROJECT_NAME}_conversions)
else()
  message(STATUS "Using standard rosidl_generate_interfaces (msg/ExampleMessage.msg, ucA) for ${PROJECT_NAME}")
  rosidl_generate_interfaces(${PROJECT_NAME}
    "msg/ExampleMessage.msg"
    DEPENDENCIES
  )
  set(CONVERSIONS_LIB "")
endif()

rosidl_get_typesupport_target(cpp_typesupport_target "${PROJECT_NAME}" "rosidl_typesupport_cpp")
rosidl_get_typesupport_target(protobuf_cpp_typesupport_target "${PROJECT_NAME}" "rosidl_typesupport_protobuf_cpp")

add_executable(example_proto_publisher ./src/example_proto_publisher.cpp)
target_link_libraries(example_proto_publisher PRIVATE rclcpp::rclcpp std_msgs::std_msgs ${cpp_typesupport_target} ${protobuf_cpp_typesupport_target} ${PROJECT_NAME}__rosidl_generator_cpp ${CONVERSIONS_LIB})

add_executable(example_proto_subscriber ./src/example_proto_subscriber.cpp)
target_link_libraries(example_proto_subscriber PRIVATE rclcpp::rclcpp std_msgs::std_msgs ${cpp_typesupport_target} ${protobuf_cpp_typesupport_target} ${PROJECT_NAME}__rosidl_generator_cpp ${CONVERSIONS_LIB})

install(TARGETS
  example_proto_publisher
  example_proto_subscriber
  DESTINATION lib/${PROJECT_NAME}
)

install(PROGRAMS
  scripts/rosbag2_reader.py
  scripts/rosbags_reader.py
  scripts/mcap_ros2_reader.py
  scripts/mcap_proto_reader.py
  DESTINATION lib/${PROJECT_NAME}
)

# if(BUILD_TESTING)
#   find_package(ament_lint_auto REQUIRED)
#   ament_lint_auto_find_test_dependencies()
# endif()

ament_export_dependencies(rosidl_default_runtime)
ament_package()
