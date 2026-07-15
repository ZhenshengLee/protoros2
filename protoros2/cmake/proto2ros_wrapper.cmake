# Copyright (c) 2023 Robotics and AI Institute LLC dba RAI Institute. All rights reserved.
#
# Override of proto2ros_vendor_package macro to inject Y-Shape architecture
# (rosidl_adapter_proto include paths, protobuf_cpp typesupport target dependencies,
# and ROS 2 Lyrical target_link_libraries compatibility).

macro(proto2ros_vendor_package target)
  set(options NO_LINT)
  set(one_value_keywords PACKAGE_NAME)
  set(
    multi_value_keywords
    PROTOS IMPORT_DIRS CONFIG_OVERLAYS ROS_DEPENDENCIES CPP_DEPENDENCIES
    CPP_INCLUDES CPP_SOURCES PYTHON_MODULES PYTHON_PACKAGES DEPENDS
  )
  cmake_parse_arguments(ARG "${options}" "${one_value_keywords}" "${multi_value_keywords}" ${ARGN})

  if(NOT ARG_PACKAGE_NAME)
    set(ARG_PACKAGE_NAME ${PROJECT_NAME})
  endif()

  get_filename_component(package_path "${ARG_PACKAGE_NAME}" ABSOLUTE)
  if(EXISTS "${package_path}/__init__.py")
    list(APPEND ARG_PYTHON_PACKAGES ${ARG_PACKAGE_NAME})
  endif()

  if(NOT ARG_CPP_INCLUDES)
    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/${ARG_PACKAGE_NAME}")
      list(APPEND ARG_CPP_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/include")
    endif()
  endif()

  if(NOT ARG_CPP_SOURCES)
    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/${ARG_PACKAGE_NAME}" AND IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src")
      file(GLOB ARG_CPP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc")
    endif()
  endif()

  set(proto2ros_generate_OPTIONS)
  if(ARG_NO_LINT)
    list(APPEND proto2ros_generate_OPTIONS NO_LINT)
  endif()
  if(ARG_DEPENDS)
    list(APPEND proto2ros_generate_OPTIONS DEPENDS ${ARG_DEPENDS})
  endif()

  proto2ros_generate(
    ${target}_messages_gen
    PROTOS ${ARG_PROTOS}
    IMPORT_DIRS ${ARG_IMPORT_DIRS}
    PACKAGE_NAME ${ARG_PACKAGE_NAME}
    CONFIG_OVERLAYS ${ARG_CONFIG_OVERLAYS}
    INTERFACES_OUT_VAR ros_messages
    PYTHON_OUT_VAR py_sources
    CPP_OUT_VAR generated_cpp_sources
    INCLUDE_OUT_VAR generated_cpp_include_dir
    ${proto2ros_generate_OPTIONS}
  )

  rosidl_generate_interfaces(
    ${target} ${ros_messages}
    DEPENDENCIES ${ARG_ROS_DEPENDENCIES} builtin_interfaces proto2ros
  )
  add_dependencies(${target} ${target}_messages_gen)

  rosidl_generated_python_package_add(
    ${target}_additional_modules
    MODULES ${ARG_PYTHON_MODULES} ${py_sources}
    PACKAGES ${ARG_PYTHON_PACKAGES}
    DESTINATION ${target}
  )

  add_library(${target}_conversions SHARED ${generated_cpp_sources} ${ARG_CPP_SOURCES})
  target_compile_features(${target}_conversions PRIVATE cxx_std_17)
  # NOTE: conversion APIs cannot ignore deprecated fields, so deprecation warnings must be disabled
  target_compile_options(${target}_conversions PRIVATE -Wno-deprecated -Wno-deprecated-declarations)
  list(APPEND build_include_directories "$<BUILD_INTERFACE:${generated_cpp_include_dir}>")
  foreach(cpp_include_dir ${ARG_CPP_INCLUDES})
    list(APPEND build_include_directories "$<BUILD_INTERFACE:${cpp_include_dir}>")
  endforeach()
  target_include_directories(${target}_conversions PUBLIC
    ${build_include_directories}
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/rosidl_adapter_proto>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/rosidl_adapter_proto/${target}/msg>"
    "$<INSTALL_INTERFACE:include/${PROJECT_NAME}>"
  )
  rosidl_get_typesupport_target(cpp_interfaces ${target} "rosidl_typesupport_cpp")
  rosidl_get_typesupport_target(protobuf_cpp_interfaces ${target} "rosidl_typesupport_protobuf_cpp")
  target_link_libraries(${target}_conversions PUBLIC
    ${cpp_interfaces}
    ${protobuf_cpp_interfaces}
    ${ARG_CPP_DEPENDENCIES}
    rclcpp::rclcpp
    builtin_interfaces::builtin_interfaces
    proto2ros::proto2ros_conversions
  )

  find_program(CLANG_TIDY_EXECUTABLE NAMES "clang-tidy")
  if(BUILD_TESTING AND NOT ARG_NO_LINT AND CLANG_TIDY_EXECUTABLE)
    list(APPEND clang_tidy_header_regexes "${generated_cpp_include_dir}/.*hpp")
    foreach(cpp_include_dir ARG_CPP_INCLUDES)
      list(APPEND clang_tidy_header_regexes "${cpp_include_dir}/.*hpp")
    endforeach()
    list(JOIN clang_tidy_header_regexes "|" clang_tidy_header_filter)
    set(CXX_CLANG_TIDY "${CLANG_TIDY_EXECUTABLE}"
      "-header-filter='^(${clang_tidy_header_filter})$'"
      "-checks=-clang-diagnostic-ignored-optimization-argument")
    set_target_properties(${target}_conversions PROPERTIES
      CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_CLANG_TIDY "${CXX_CLANG_TIDY}")
  endif()

  set(generated_header_files ${generated_cpp_sources})
  list(FILTER generated_header_files INCLUDE REGEX ".*\.hpp$")
  install(
    FILES ${generated_header_files}
    DESTINATION include/${PROJECT_NAME}/${ARG_PACKAGE_NAME}/
  )
  foreach(cpp_include_dir ${ARG_CPP_INCLUDES})
    install(
      DIRECTORY ${cpp_include_dir}/
      DESTINATION include/${PROJECT_NAME}/
    )
  endforeach()

  install(
    TARGETS ${target}_conversions
    EXPORT ${PROJECT_NAME}
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
  )
  ament_export_dependencies(builtin_interfaces proto2ros rclcpp)
  ament_export_targets(${PROJECT_NAME})
endmacro()
