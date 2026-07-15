# Copyright (c) 2023 Robotics and AI Institute LLC dba RAI Institute. All rights reserved.
#
# Override of proto2ros_vendor_package macro to inject Y-Shape architecture
# (rosidl_adapter_proto include paths, protobuf_cpp typesupport target dependencies,
# and ROS 2 Lyrical target_link_libraries compatibility).

macro(proto2ros_vendor_package target)
  set(options NO_LINT PROTO_SSOT_ONLY)
  set(one_value_keywords PACKAGE_NAME)
  set(
    multi_value_keywords
    PROTOS IMPORT_DIRS CONFIG_OVERLAYS ROS_DEPENDENCIES CPP_DEPENDENCIES
    CPP_INCLUDES CPP_SOURCES PYTHON_MODULES PYTHON_PACKAGES DEPENDS
  )
  cmake_parse_arguments(ARG "${options}" "${one_value_keywords}" "${multi_value_keywords}" ${ARGN})

  set(_is_proto_ssot_only FALSE)
  if(ARG_PROTOS AND (ARG_PROTO_SSOT_ONLY OR PROTO_SSOT_ONLY))
    set(_is_proto_ssot_only TRUE)
  endif()

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

  # diable lint of mypy.ini
  if(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/proto2ros_generate/mypy.ini")
    file(READ "${CMAKE_CURRENT_BINARY_DIR}/proto2ros_generate/mypy.ini" _mypy_content)
    string(REPLACE "disable_error_code = var-annotated" "disable_error_code = var-annotated, attr-defined, name-defined" _mypy_content "${_mypy_content}")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/proto2ros_generate/mypy.ini" "${_mypy_content}")
  endif()

  if(_is_proto_ssot_only)
    # ucB.2: Remove rosidl(msg) support (rosidl_adapter_proto) and rosidl_typesupport_protobuf_cpp
    # from the standard interface generation pipeline when .proto is the Single Source of Truth (SSOT).
    if(NOT DEFINED rosidl_adapter_proto_TEMPLATE_DIR OR NOT EXISTS "${rosidl_adapter_proto_TEMPLATE_DIR}/rosidl_adapter_proto__visibility_control.h.in")
      if(EXISTS "/gw_demo/packages/protoros2/3rdparty/rosidl_typesupport_protobuf/rosidl_adapter_proto/resource/rosidl_adapter_proto__visibility_control.h.in")
        set(rosidl_adapter_proto_TEMPLATE_DIR "/gw_demo/packages/protoros2/3rdparty/rosidl_typesupport_protobuf/rosidl_adapter_proto/resource")
      elseif(EXISTS "/opt/ros/lyrical/share/rosidl_adapter_proto/resource/rosidl_adapter_proto__visibility_control.h.in")
        set(rosidl_adapter_proto_TEMPLATE_DIR "/opt/ros/lyrical/share/rosidl_adapter_proto/resource")
      endif()
    endif()

    set(_saved_rosidl_exts "${AMENT_EXTENSIONS_rosidl_generate_idl_interfaces}")
    if(DEFINED AMENT_EXTENSIONS_rosidl_generate_idl_interfaces)
      set(_filtered_rosidl_exts "")
      foreach(_ext IN LISTS AMENT_EXTENSIONS_rosidl_generate_idl_interfaces)
        if(NOT _ext MATCHES "^rosidl_adapter_proto:" AND
           NOT _ext MATCHES "^rosidl_typesupport_protobuf_c:" AND
           NOT _ext MATCHES "^rosidl_typesupport_protobuf_cpp:")
          list(APPEND _filtered_rosidl_exts "${_ext}")
        endif()
      endforeach()
      set(AMENT_EXTENSIONS_rosidl_generate_idl_interfaces "${_filtered_rosidl_exts}")
    endif()

    # Compile SSOT .proto files directly to C++ headers and sources using protoc
    set(proto_ssot_output_dir "${CMAKE_CURRENT_BINARY_DIR}/proto2ros_ssot/${target}")
    file(MAKE_DIRECTORY "${proto_ssot_output_dir}")
    set(proto_ssot_generated_cpp "")
    set(proto_ssot_generated_h "")
    get_executable_path(PROTOC_EXECUTABLE protobuf::protoc CONFIGURE)
    foreach(proto_file ${ARG_PROTOS})
      get_filename_component(abs_proto_file "${proto_file}" ABSOLUTE)
      get_filename_component(proto_rel_dir "${proto_file}" DIRECTORY)
      get_filename_component(proto_stem "${proto_file}" NAME_WE)
      set(pb_cc "${proto_ssot_output_dir}/${proto_rel_dir}/${proto_stem}.pb.cc")
      set(pb_h "${proto_ssot_output_dir}/${proto_rel_dir}/${proto_stem}.pb.h")
      file(MAKE_DIRECTORY "${proto_ssot_output_dir}/${proto_rel_dir}")
      list(APPEND proto_ssot_generated_cpp "${pb_cc}")
      list(APPEND proto_ssot_generated_h "${pb_h}")
      add_custom_command(
        OUTPUT "${pb_cc}" "${pb_h}"
        COMMAND ${PROTOC_EXECUTABLE} "-I${CMAKE_CURRENT_SOURCE_DIR}" "--cpp_out=dllexport_decl=ROSIDL_ADAPTER_PROTO_PUBLIC__${PROJECT_NAME}:${proto_ssot_output_dir}" "${abs_proto_file}"
        DEPENDS "${abs_proto_file}"
        COMMENT "Generating SSOT Protobuf C++ header/source for ${proto_file}"
        VERBATIM
      )
    endforeach()

    # Generate visibility control header expected by downstream targets
    set(rosidl_adapter_proto_VISIBILITY_CONTROL_HEADER "${proto_ssot_output_dir}/msg/rosidl_adapter_proto__visibility_control.h")
    file(MAKE_DIRECTORY "${proto_ssot_output_dir}/msg")
    file(MAKE_DIRECTORY "${proto_ssot_output_dir}/${target}/msg")
    string(TOUPPER "${PROJECT_NAME}" PROJECT_NAME_UPPER)
    if(EXISTS "${rosidl_adapter_proto_TEMPLATE_DIR}/rosidl_adapter_proto__visibility_control.h.in")
      configure_file(
        "${rosidl_adapter_proto_TEMPLATE_DIR}/rosidl_adapter_proto__visibility_control.h.in"
        "${rosidl_adapter_proto_VISIBILITY_CONTROL_HEADER}"
        @ONLY
      )
      configure_file(
        "${rosidl_adapter_proto_TEMPLATE_DIR}/rosidl_adapter_proto__visibility_control.h.in"
        "${proto_ssot_output_dir}/${target}/msg/rosidl_adapter_proto__visibility_control.h"
        @ONLY
      )
    endif()
  endif()

  rosidl_generate_interfaces(
    ${target} ${ros_messages}
    DEPENDENCIES ${ARG_ROS_DEPENDENCIES} builtin_interfaces proto2ros
  )
  add_dependencies(${target} ${target}_messages_gen)

  if(_is_proto_ssot_only)
    if(DEFINED _saved_rosidl_exts)
      set(AMENT_EXTENSIONS_rosidl_generate_idl_interfaces "${_saved_rosidl_exts}")
    endif()

    # ucB.2 Second Engine: Explicitly generate rosidl_typesupport_protobuf_cpp linked against SSOT .pb.cc
    if(NOT TARGET ${target}__rosidl_typesupport_protobuf_cpp)
      find_package(rosidl_typesupport_protobuf_cpp REQUIRED)
      set(_ts_output_path "${CMAKE_CURRENT_BINARY_DIR}/rosidl_typesupport_protobuf_cpp/${target}")
      set(_ts_generated_files "")
      foreach(_abs_idl_file ${rosidl_generate_interfaces_ABS_IDL_FILES})
        get_filename_component(_parent_folder "${_abs_idl_file}" DIRECTORY)
        get_filename_component(_parent_folder "${_parent_folder}" NAME)
        get_filename_component(_idl_name "${_abs_idl_file}" NAME_WE)
        string_camel_case_to_lower_case_underscore("${_idl_name}" _header_name)
        list(APPEND _ts_generated_files
          "${_ts_output_path}/${_parent_folder}/${_header_name}__typeadapter_protobuf_cpp.hpp"
          "${_ts_output_path}/${_parent_folder}/detail/${_header_name}__rosidl_typesupport_protobuf_cpp.hpp"
          "${_ts_output_path}/${_parent_folder}/detail/${_header_name}__rosidl_typesupport_protobuf_cpp.cpp"
        )
      endforeach()

      set(_dependencies "")
      foreach(_pkg_name ${rosidl_generate_interfaces_DEPENDENCY_PACKAGE_NAMES})
        foreach(_idl_file ${${_pkg_name}_IDL_FILES})
          set(_abs_idl_file "${${_pkg_name}_DIR}/../${_idl_file}")
          normalize_path(_abs_idl_file "${_abs_idl_file}")
          list(APPEND _dependencies "${_pkg_name}:${_abs_idl_file}")
        endforeach()
      endforeach()

      set(generator_arguments_file "${CMAKE_CURRENT_BINARY_DIR}/rosidl_typesupport_protobuf_cpp__arguments.json")
      rosidl_write_generator_arguments(
        "${generator_arguments_file}"
        PACKAGE_NAME "${target}"
        IDL_TUPLES "${rosidl_generate_interfaces_IDL_TUPLES}"
        ROS_INTERFACE_DEPENDENCIES "${_dependencies}"
        OUTPUT_DIR "${_ts_output_path}"
        TEMPLATE_DIR "${rosidl_typesupport_protobuf_cpp_TEMPLATE_DIR}"
        TARGET_DEPENDENCIES "${rosidl_typesupport_protobuf_cpp_BIN}" ${rosidl_typesupport_protobuf_cpp_GENERATOR_FILES}
      )

      add_custom_command(
        OUTPUT ${_ts_generated_files}
        COMMAND ${PYTHON_EXECUTABLE} "${rosidl_typesupport_protobuf_cpp_BIN}"
        --generator-arguments-file "${generator_arguments_file}"
        DEPENDS ${rosidl_generate_interfaces_ABS_IDL_FILES} "${rosidl_typesupport_protobuf_cpp_BIN}" ${rosidl_typesupport_protobuf_cpp_GENERATOR_FILES}
        COMMENT "Generating SSOT C++ type support for Protobuf"
        VERBATIM
      )

      set(_visibility_control_file "${_ts_output_path}/msg/rosidl_typesupport_protobuf_cpp__visibility_control.h")
      string(TOUPPER "${target}" PROJECT_NAME_UPPER)
      if(EXISTS "${rosidl_typesupport_protobuf_cpp_TEMPLATE_DIR}/rosidl_typesupport_protobuf_cpp__visibility_control.h.in")
        configure_file(
          "${rosidl_typesupport_protobuf_cpp_TEMPLATE_DIR}/rosidl_typesupport_protobuf_cpp__visibility_control.h.in"
          "${_visibility_control_file}"
          @ONLY
        )
      endif()

      add_library(${target}__rosidl_typesupport_protobuf_cpp SHARED
        ${_ts_generated_files}
        ${proto_ssot_generated_cpp}
      )
      set_target_properties(${target}__rosidl_typesupport_protobuf_cpp PROPERTIES
        OUTPUT_NAME "${target}__rosidl_typesupport_protobuf_cpp"
        CXX_STANDARD 14
      )
      if(WIN32)
        target_compile_definitions(${target}__rosidl_typesupport_protobuf_cpp PRIVATE
          "ROSIDL_TYPESUPPORT_PROTOBUF_BUILDING_DLL" "ROSIDL_TYPESUPPORT_PROTOBUF_CPP_BUILDING_DLL__${target}")
      endif()

      if(NOT WIN32)
        set(_target_compile_flags "-Wall -Wextra -Wpedantic")
      else()
        set(_target_compile_flags "/W4")
      endif()
      string(REPLACE ";" " " _target_compile_flags "${_target_compile_flags}")
      set_target_properties(${target}__rosidl_typesupport_protobuf_cpp PROPERTIES
        COMPILE_FLAGS "${_target_compile_flags}"
      )

      target_include_directories(${target}__rosidl_typesupport_protobuf_cpp PUBLIC
        "$<BUILD_INTERFACE:${proto_ssot_output_dir}>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/proto2ros_ssot>"
        "$<BUILD_INTERFACE:${proto_ssot_output_dir}/msg>"
        "$<BUILD_INTERFACE:${_ts_output_path}>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/rosidl_typesupport_protobuf_cpp>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/rosidl_generator_cpp>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/rosidl_generator_c>"
        "$<INSTALL_INTERFACE:include/${PROJECT_NAME}>"
      )
      if(NOT WIN32)
        target_compile_options(${target}__rosidl_typesupport_protobuf_cpp PRIVATE "-include${rosidl_adapter_proto_VISIBILITY_CONTROL_HEADER}")
      else()
        target_compile_options(${target}__rosidl_typesupport_protobuf_cpp PRIVATE "/FI\"${rosidl_adapter_proto_VISIBILITY_CONTROL_HEADER}\"")
      endif()
      target_link_libraries(${target}__rosidl_typesupport_protobuf_cpp PUBLIC
        rmw::rmw
        rosidl_typesupport_interface::rosidl_typesupport_interface
        rosidl_typesupport_protobuf_cpp::rosidl_typesupport_protobuf_cpp
      )
      add_dependencies(${target}__rosidl_typesupport_protobuf_cpp ${target}_messages_gen)

      set_property(GLOBAL PROPERTY "rosidl_get_typesupport_target_${target}_rosidl_typesupport_protobuf_cpp" "${target}__rosidl_typesupport_protobuf_cpp")
      install(
        TARGETS ${target}__rosidl_typesupport_protobuf_cpp
        EXPORT ${PROJECT_NAME}
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
      )
      install(
        DIRECTORY "${_ts_output_path}/"
        DESTINATION "include/${PROJECT_NAME}"
        PATTERN "*.h"
        PATTERN "*.hpp"
      )
      install(
        DIRECTORY "${proto_ssot_output_dir}/"
        DESTINATION "include/${PROJECT_NAME}"
        PATTERN "*.h"
        PATTERN "*.hpp"
      )
    endif()
  endif()

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
  if(_is_proto_ssot_only)
    set(conversions_proto_include_dir "${proto_ssot_output_dir}")
    if(NOT WIN32)
      target_compile_options(${target}_conversions PRIVATE "-include${rosidl_adapter_proto_VISIBILITY_CONTROL_HEADER}")
    else()
      target_compile_options(${target}_conversions PRIVATE "/FI\"${rosidl_adapter_proto_VISIBILITY_CONTROL_HEADER}\"")
    endif()
    list(APPEND build_include_directories
      "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/proto2ros_ssot>"
      "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/rosidl_typesupport_protobuf_cpp>"
      "$<BUILD_INTERFACE:${_ts_output_path}>"
    )
  else()
    set(conversions_proto_include_dir "${CMAKE_CURRENT_BINARY_DIR}/rosidl_adapter_proto")
  endif()
  target_include_directories(${target}_conversions PUBLIC
    ${build_include_directories}
    "$<BUILD_INTERFACE:${conversions_proto_include_dir}>"
    "$<BUILD_INTERFACE:${conversions_proto_include_dir}/msg>"
    "$<BUILD_INTERFACE:${conversions_proto_include_dir}/${target}/msg>"
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

  if(_is_proto_ssot_only)
    install(
      DIRECTORY ${proto_ssot_output_dir}/
      DESTINATION include/${PROJECT_NAME}
      FILES_MATCHING PATTERN "*.h"
    )
  endif()

  install(
    TARGETS ${target}_conversions
    EXPORT ${PROJECT_NAME}
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
  )
  ament_export_dependencies(builtin_interfaces proto2ros rosidl_typesupport_protobuf_cpp rclcpp)
  ament_export_targets(${PROJECT_NAME})
endmacro()
