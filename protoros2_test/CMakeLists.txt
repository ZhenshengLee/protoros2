cmake_minimum_required(VERSION 3.20)
project(protoros2_test)

# Default to C++17
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(proto2ros REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(rosidl_adapter_proto REQUIRED)
find_package(rosidl_typesupport_protobuf_cpp REQUIRED)
find_package(protoros2 REQUIRED)
find_package(iceoryx2-cxx REQUIRED)
find_package(iceoryx2-c REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)

# colcon test 不继承 shell 的 LD_LIBRARY_PATH，从 find_package 导入的 target
# 解析 iceoryx2 动态库目录，注入到各 gtest 的 ENVIRONMENT
get_target_property(_iox2_lib iceoryx2-cxx::shared-lib-cxx IMPORTED_LOCATION)
if(NOT _iox2_lib)
  get_target_property(_iox2_lib iceoryx2-cxx::shared-lib-cxx IMPORTED_LOCATION_NOCONFIG)
endif()
get_filename_component(iox2_lib_dir "${_iox2_lib}" DIRECTORY)

option(PROTO_SSOT "Use Proto file coexisting with rosidl_typesupport_protobuf (ucB.1)" OFF)
option(PROTO_SSOT_ONLY "Use Proto file as the ONLY SSOT directly, bypassing rosidl_adapter_proto (ucB.2)" OFF)
option(BUILD_ENTERPRISE_TESTS "Build EnterpriseNode integration tests" ON)

if(PROTO_SSOT AND PROTO_SSOT_ONLY)
  message(FATAL_ERROR "PROTO_SSOT (ucB.1) and PROTO_SSOT_ONLY (ucB.2) cannot both be ON simultaneously!")
endif()

# ---------------------------------------------------------------------------
# Unit tests (only when BUILD_TESTING=ON) and (PROTO_SSOT_ONLY mode = ucB.2)
# ---------------------------------------------------------------------------
if(BUILD_TESTING AND PROTO_SSOT_ONLY)
  # find_package(ament_lint_auto REQUIRED)
  # ament_lint_auto_find_test_dependencies()
  find_package(ament_cmake_gtest REQUIRED)

  # All .proto files are the single source of truth; rosidl_adapter_proto is bypassed.
  proto2ros_vendor_package(${PROJECT_NAME}
    PROTO_SSOT_ONLY
    IMPORT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
    PROTOS
      # Core type test messages
      "msg/BasicTypes.proto"
      "msg/ArrayTypes.proto"
      "msg/SubMessage.proto"
      "msg/NestedMessage.proto"
      # Dual-engine consistency test messages
      "msg/NestedConsistencyMessage.proto"
      "msg/TestConsistencyMessage.proto"
  )

  rosidl_get_typesupport_target(cpp_interfaces ${PROJECT_NAME} "rosidl_typesupport_cpp")
  rosidl_get_typesupport_target(protobuf_cpp_interfaces ${PROJECT_NAME} "rosidl_typesupport_protobuf_cpp")

  # Helper macro: add a gtest target linked against the common interface libraries
  macro(protoros2_test_add_gtest target_name source_file)
    ament_add_gtest(${target_name} ${source_file})
    if(TARGET ${target_name})
      target_link_libraries(${target_name}
        rclcpp::rclcpp
        std_msgs::std_msgs
        protoros2::protoros2
        ${cpp_interfaces}
        ${protobuf_cpp_interfaces}
        ${PROJECT_NAME}__rosidl_generator_cpp
        ${PROJECT_NAME}_conversions
      )
      set_tests_properties(${target_name} PROPERTIES
        ENVIRONMENT "LD_LIBRARY_PATH=${iox2_lib_dir}:$ENV{LD_LIBRARY_PATH}")
    endif()
  endmacro()

  # -- Core type conversion tests --
  protoros2_test_add_gtest(test_basic_types_conversions
    test/test_basic_types_conversions.cpp)

  protoros2_test_add_gtest(test_array_types_conversions
    test/test_array_types_conversions.cpp)

  protoros2_test_add_gtest(test_nested_message_conversions
    test/test_nested_message_conversions.cpp)

  protoros2_test_add_gtest(test_typeadapter_rclcpp_integration
    test/test_typeadapter_rclcpp_integration.cpp)

  # -- Dual-engine consistency test --
  # Verifies proto2ros and rosidl_typesupport_protobuf_cpp produce identical output.
  ament_add_gtest(test_type_mapping_consistency
    test/test_type_mapping_consistency.cpp)
  if(TARGET test_type_mapping_consistency)
    set(_consistency_proto_inc_dir
      "${CMAKE_CURRENT_BINARY_DIR}/proto2ros_ssot/${PROJECT_NAME}")
    set(_consistency_visibility_header
      "${_consistency_proto_inc_dir}/msg/rosidl_adapter_proto__visibility_control.h")
    target_compile_options(test_type_mapping_consistency PRIVATE
      "-include${_consistency_visibility_header}")
    target_include_directories(test_type_mapping_consistency PRIVATE
      "${_consistency_proto_inc_dir}/.."
      "${_consistency_proto_inc_dir}"
      "${_consistency_proto_inc_dir}/msg"
      "${CMAKE_CURRENT_BINARY_DIR}/proto2ros_generate/include"
      "${CMAKE_CURRENT_BINARY_DIR}/rosidl_generator_cpp"
      "${CMAKE_CURRENT_BINARY_DIR}/rosidl_typesupport_protobuf_cpp"
    )
    target_link_libraries(test_type_mapping_consistency
      rclcpp::rclcpp
      iceoryx2-cxx::shared-lib-cxx
      iceoryx2-c::shared-lib
      ${cpp_interfaces}
      ${protobuf_cpp_interfaces}
      ${PROJECT_NAME}__rosidl_generator_cpp
      ${PROJECT_NAME}_conversions
    )
    if(TARGET ${PROJECT_NAME}__rosidl_typesupport_protobuf_cpp)
      add_dependencies(test_type_mapping_consistency
        ${PROJECT_NAME}__rosidl_typesupport_protobuf_cpp)
    endif()
    if(TARGET ${PROJECT_NAME}_conversions)
      add_dependencies(test_type_mapping_consistency ${PROJECT_NAME}_conversions)
    endif()
    set_tests_properties(test_type_mapping_consistency PROPERTIES
      ENVIRONMENT "LD_LIBRARY_PATH=${iox2_lib_dir}:$ENV{LD_LIBRARY_PATH}")
  endif()
endif()

if(BUILD_TESTING AND BUILD_ENTERPRISE_TESTS AND NOT PROTO_SSOT_ONLY)
  find_package(ament_cmake_gtest REQUIRED)
  # Generate BasicTypes.proto specifically for enterprise node testing when PROTO_SSOT_ONLY is OFF
  proto2ros_vendor_package(${PROJECT_NAME}
    PROTO_SSOT_ONLY
    IMPORT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
    PROTOS "msg/BasicTypes.proto" "msg/BenchBytes.proto"
  )
  rosidl_get_typesupport_target(cpp_interfaces ${PROJECT_NAME} "rosidl_typesupport_cpp")
  rosidl_get_typesupport_target(protobuf_cpp_interfaces ${PROJECT_NAME} "rosidl_typesupport_protobuf_cpp")

  macro(protoros2_test_add_gtest target_name source_file)
    ament_add_gtest(${target_name} ${source_file})
    if(TARGET ${target_name})
      target_link_libraries(${target_name}
        rclcpp::rclcpp
        std_msgs::std_msgs
        protoros2::protoros2
        ${cpp_interfaces}
        ${protobuf_cpp_interfaces}
        ${PROJECT_NAME}__rosidl_generator_cpp
        ${PROJECT_NAME}_conversions
      )
      set_tests_properties(${target_name} PROPERTIES
        ENVIRONMENT "LD_LIBRARY_PATH=${iox2_lib_dir}:$ENV{LD_LIBRARY_PATH}")
    endif()
  endmacro()
endif()

if (BUILD_TESTING AND (PROTO_SSOT_ONLY OR BUILD_ENTERPRISE_TESTS))
  # iceoryx2 backend is daemonless (no RouDi needed), integration tests re-enabled
  protoros2_test_add_gtest(test_enterprise_node_integration
      test/test_enterprise_node_integration.cpp)

  protoros2_test_add_gtest(test_enterprise_node_paradigms
      test/test_enterprise_node_paradigms.cpp)

  protoros2_test_add_gtest(test_waitable test/test_waitable.cpp)

  protoros2_test_add_gtest(test_zero_copy_parse test/test_zero_copy_parse.cpp)
endif()

ament_package()
