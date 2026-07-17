# Copyright (c) 2023 Robotics and AI Institute LLC dba RAI Institute. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Override of proto2ros_vendor_package macro to inject Y-Shape architecture
# (rosidl_adapter_proto include paths, protobuf_cpp typesupport target dependencies,
# and ROS 2 Lyrical target_link_libraries compatibility).

macro(proto2ros_vendor_package target)
  set(options NO_LINT PROTO_SSOT_ONLY PROTO_SSOT)
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

  # ---------------------------------------------------------------------------
  # Validation & Verification Guard: Help users catch configuration errors early
  # ---------------------------------------------------------------------------
  if((ARG_PROTO_SSOT_ONLY OR PROTO_SSOT_ONLY) AND (ARG_PROTO_SSOT OR PROTO_SSOT))
    message(FATAL_ERROR "[proto2ros_vendor_package] In package '${PROJECT_NAME}': Both PROTO_SSOT_ONLY and PROTO_SSOT options are enabled. "
      "PROTO_SSOT_ONLY completely replaces the ROS IDL pipeline with SSOT Protobuf, while PROTO_SSOT enables three-state/coexistence mode. "
      "They cannot be enabled simultaneously. Please choose only one.")
  endif()

  if(ARG_PROTOS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/package.xml")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/package.xml" _package_xml_content)
    if(NOT _package_xml_content MATCHES "<member_of_group>rosidl_interface_packages</member_of_group>")
      message(FATAL_ERROR "[proto2ros_vendor_package] In package '${PROJECT_NAME}': package.xml is missing "
        "'<member_of_group>rosidl_interface_packages</member_of_group>'. This group tag is required by ROS 2 "
        "when generating message interfaces.")
    endif()
  endif()

  if(ARG_PROTOS)
    foreach(_proto_file IN LISTS ARG_PROTOS)
      get_filename_component(_abs_proto_file "${_proto_file}" ABSOLUTE)
      if(NOT EXISTS "${_abs_proto_file}")
        message(FATAL_ERROR "[proto2ros_vendor_package] In package '${PROJECT_NAME}': Specified proto file '${_proto_file}' "
          "(${_abs_proto_file}) does not exist on disk!")
      endif()

      file(READ "${_abs_proto_file}" _proto_content)
      if(NOT _proto_content MATCHES "package[ \t]+([a-zA-Z0-9_.]+)[ \t]*;")
        message(FATAL_ERROR "[proto2ros_vendor_package] In package '${PROJECT_NAME}': Proto file '${_proto_file}' is missing a valid "
          "'package <name>;' statement!")
      endif()
      set(_proto_package "${CMAKE_MATCH_1}")

      if(NOT _proto_package MATCHES "\\.pb$")
        message(FATAL_ERROR "[proto2ros_vendor_package] In package '${PROJECT_NAME}': Proto file '${_proto_file}' declares package "
          "'${_proto_package}', which does not end with '.pb'. To prevent C++ namespace collisions between generated ROS structures and "
          "Protobuf classes, the package statement MUST end with '.pb' (expected format: '${ARG_PACKAGE_NAME}.<folder_name>.pb;').")
      endif()

      string(FIND "${_proto_package}" "${ARG_PACKAGE_NAME}." _pkg_prefix_idx)
      if(NOT _pkg_prefix_idx EQUAL 0)
        message(FATAL_ERROR "[proto2ros_vendor_package] In package '${PROJECT_NAME}': Proto file '${_proto_file}' declares package "
          "'${_proto_package}', which does not match the ROS 2 package prefix '${ARG_PACKAGE_NAME}.' "
          "(expected format: '${ARG_PACKAGE_NAME}.<folder_name>.pb;').")
      endif()

      if(_is_proto_ssot_only OR ARG_PROTO_SSOT OR PROTO_SSOT)
        get_filename_component(_proto_stem "${_proto_file}" NAME_WE)
        if(NOT _proto_content MATCHES "(\n|^)[ \t]*message[ \t]+${_proto_stem}([ \t\r\n]+|\\{)")
          message(WARNING "[proto2ros_vendor_package] In package '${PROJECT_NAME}': "
            "Proto file '${_proto_file}' does not define a top-level "
            "'message ${_proto_stem}'. In SSOT mode, each .proto file generating a ROS .msg structure should define a top-level message "
            "whose name exactly matches the file stem (${_proto_stem}) so that generated C++ headers (${_proto_stem}.pb.h) align "
            "with rosidl_typesupport_protobuf_cpp expectations.")
        endif()
      endif()
    endforeach()
  endif()
  # ---------------------------------------------------------------------------

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
    string(REPLACE "disable_error_code = var-annotated" "disable_error_code = var-annotated, attr-defined, name-defined"
      _mypy_content "${_mypy_content}")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/proto2ros_generate/mypy.ini" "${_mypy_content}")
  endif()

  if(_is_proto_ssot_only)
    # ucB.2: Remove rosidl(msg) support (rosidl_adapter_proto) and rosidl_typesupport_protobuf_cpp
    # from the standard interface generation pipeline when .proto is the Single Source of Truth (SSOT).
    if(NOT DEFINED rosidl_adapter_proto_TEMPLATE_DIR
        OR NOT EXISTS "${rosidl_adapter_proto_TEMPLATE_DIR}/rosidl_adapter_proto__visibility_control.h.in")
      set(_local_template_in
        "/gw_demo/packages/protoros2/3rdparty/rosidl_typesupport_protobuf/rosidl_adapter_proto/resource/rosidl_adapter_proto__visibility_control.h.in")
      if(EXISTS "${_local_template_in}")
        set(rosidl_adapter_proto_TEMPLATE_DIR
          "/gw_demo/packages/protoros2/3rdparty/rosidl_typesupport_protobuf/rosidl_adapter_proto/resource")
      elseif(EXISTS "/opt/ros/lyrical/share/rosidl_adapter_proto/resource/rosidl_adapter_proto__visibility_control.h.in")
        set(rosidl_adapter_proto_TEMPLATE_DIR
          "/opt/ros/lyrical/share/rosidl_adapter_proto/resource")
      endif()
    endif()

    set(_saved_rosidl_exts "${AMENT_EXTENSIONS_rosidl_generate_idl_interfaces}")
    if(DEFINED AMENT_EXTENSIONS_rosidl_generate_idl_interfaces)
      set(_filtered_rosidl_exts "")
      foreach(_ext IN LISTS AMENT_EXTENSIONS_rosidl_generate_idl_interfaces)
        if(NOT _ext MATCHES "^rosidl_adapter_proto:"
            AND NOT _ext MATCHES "^rosidl_typesupport_protobuf_c:"
            AND NOT _ext MATCHES "^rosidl_typesupport_protobuf_cpp:")
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
        COMMAND ${PROTOC_EXECUTABLE} "-I${CMAKE_CURRENT_SOURCE_DIR}"
          "--cpp_out=dllexport_decl=ROSIDL_ADAPTER_PROTO_PUBLIC__${PROJECT_NAME}:${proto_ssot_output_dir}" "${abs_proto_file}"
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
        DEPENDS ${rosidl_generate_interfaces_ABS_IDL_FILES} "${rosidl_typesupport_protobuf_cpp_BIN}"
          ${rosidl_typesupport_protobuf_cpp_GENERATOR_FILES}
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
        target_compile_options(${target}__rosidl_typesupport_protobuf_cpp PRIVATE
          "-include${rosidl_adapter_proto_VISIBILITY_CONTROL_HEADER}")
      else()
        target_compile_options(${target}__rosidl_typesupport_protobuf_cpp PRIVATE
          "/FI\"${rosidl_adapter_proto_VISIBILITY_CONTROL_HEADER}\"")
      endif()
      target_link_libraries(${target}__rosidl_typesupport_protobuf_cpp PUBLIC
        rmw::rmw
        rosidl_typesupport_interface::rosidl_typesupport_interface
        rosidl_typesupport_protobuf_cpp::rosidl_typesupport_protobuf_cpp
        ${target}__rosidl_generator_cpp
        ${Protobuf_LIBRARY}
        protobuf::libprotobuf
      )
      add_dependencies(${target}__rosidl_typesupport_protobuf_cpp ${target}_messages_gen)

      set_property(GLOBAL PROPERTY "rosidl_get_typesupport_target_${target}_rosidl_typesupport_protobuf_cpp"
        "${target}__rosidl_typesupport_protobuf_cpp")
      install(
        TARGETS ${target}__rosidl_typesupport_protobuf_cpp
        EXPORT export_${target}__rosidl_typesupport_protobuf_cpp
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
      )
      # Install generated typesupport headers into the ROS 2 Lyrical convention path:
      # include/<pkg>/<pkg>/msg/  (double package name layer)
      # _ts_output_path = .../rosidl_typesupport_protobuf_cpp/<target>, which already contains
      # the <target> layer. Install the *parent* dir so the layout becomes:
      #   include/${PROJECT_NAME}/<target>/msg/xxx.hpp
      install(
        DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/rosidl_typesupport_protobuf_cpp/"
        DESTINATION "include/${PROJECT_NAME}"
        PATTERN "*.cpp" EXCLUDE
        PATTERN "*.h"
        PATTERN "*.hpp"
      )
      # Install SSOT protobuf-generated .pb.h headers.
      # proto_ssot_output_dir = .../proto2ros_ssot/<target>; its msg/ subdir holds the real
      # headers. The sibling <target>/msg/ dir is an internal visibility-control copy and must
      # NOT be installed (it would create a spurious triple-nested path).
      install(
        DIRECTORY "${proto_ssot_output_dir}/msg/"
        DESTINATION "include/${PROJECT_NAME}/${target}/msg"
        PATTERN "*.h"
        PATTERN "*.hpp"
      )
      ament_export_include_directories("include/${PROJECT_NAME}")
      rosidl_export_typesupport_libraries(__rosidl_typesupport_protobuf_cpp
        ${target}__rosidl_typesupport_protobuf_cpp)
      rosidl_export_typesupport_targets(__rosidl_typesupport_protobuf_cpp
        ${target}__rosidl_typesupport_protobuf_cpp)
      ament_export_targets(export_${target}__rosidl_typesupport_protobuf_cpp)

      # ucB.2 C Engine: Explicitly generate rosidl_typesupport_protobuf_c linked against SSOT .pb.cc
      if(NOT TARGET ${target}__rosidl_typesupport_protobuf_c)
        find_package(rosidl_typesupport_protobuf_c REQUIRED)
        set(_ts_c_output_path "${CMAKE_CURRENT_BINARY_DIR}/rosidl_typesupport_protobuf_c/${target}")
        set(_ts_c_generated_files "")
        foreach(_abs_idl_file ${rosidl_generate_interfaces_ABS_IDL_FILES})
          get_filename_component(_parent_folder "${_abs_idl_file}" DIRECTORY)
          get_filename_component(_parent_folder "${_parent_folder}" NAME)
          get_filename_component(_idl_name "${_abs_idl_file}" NAME_WE)
          string_camel_case_to_lower_case_underscore("${_idl_name}" _header_name)
          list(APPEND _ts_c_generated_files
            "${_ts_c_output_path}/${_parent_folder}/detail/${_header_name}__rosidl_typesupport_protobuf_c.hpp"
            "${_ts_c_output_path}/${_parent_folder}/detail/${_header_name}__rosidl_typesupport_protobuf_c.cpp"
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

        set(generator_arguments_file_c "${CMAKE_CURRENT_BINARY_DIR}/rosidl_typesupport_protobuf_c__arguments.json")
        rosidl_write_generator_arguments(
          "${generator_arguments_file_c}"
          PACKAGE_NAME "${target}"
          IDL_TUPLES "${rosidl_generate_interfaces_IDL_TUPLES}"
          ROS_INTERFACE_DEPENDENCIES "${_dependencies}"
          OUTPUT_DIR "${_ts_c_output_path}"
          TEMPLATE_DIR "${rosidl_typesupport_protobuf_c_TEMPLATE_DIR}"
          TARGET_DEPENDENCIES "${rosidl_typesupport_protobuf_c_BIN}" ${rosidl_typesupport_protobuf_c_GENERATOR_FILES}
        )

        add_custom_command(
          OUTPUT ${_ts_c_generated_files}
          COMMAND ${PYTHON_EXECUTABLE} "${rosidl_typesupport_protobuf_c_BIN}"
          --generator-arguments-file "${generator_arguments_file_c}"
          DEPENDS ${rosidl_generate_interfaces_ABS_IDL_FILES} "${rosidl_typesupport_protobuf_c_BIN}" ${rosidl_typesupport_protobuf_c_GENERATOR_FILES}
          COMMENT "Generating SSOT C type support for Protobuf"
          VERBATIM
        )

        set(_visibility_control_file_c "${_ts_c_output_path}/msg/rosidl_typesupport_protobuf_c__visibility_control.h")
        string(TOUPPER "${target}" PROJECT_NAME_UPPER)
        if(EXISTS "${rosidl_typesupport_protobuf_c_TEMPLATE_DIR}/rosidl_typesupport_protobuf_c__visibility_control.h.in")
          configure_file(
            "${rosidl_typesupport_protobuf_c_TEMPLATE_DIR}/rosidl_typesupport_protobuf_c__visibility_control.h.in"
            "${_visibility_control_file_c}"
            @ONLY
          )
        endif()

        add_library(${target}__rosidl_typesupport_protobuf_c SHARED
          ${_ts_c_generated_files}
          ${proto_ssot_generated_cpp}
        )
        set_target_properties(${target}__rosidl_typesupport_protobuf_c PROPERTIES
          OUTPUT_NAME "${target}__rosidl_typesupport_protobuf_c"
          CXX_STANDARD 14
        )
        if(WIN32)
          target_compile_definitions(${target}__rosidl_typesupport_protobuf_c PRIVATE
            "ROSIDL_TYPESUPPORT_PROTOBUF_BUILDING_DLL" "ROSIDL_TYPESUPPORT_PROTOBUF_C_BUILDING_DLL__${target}")
        endif()

        if(NOT WIN32)
          set(_target_compile_flags "-Wall -Wextra -Wpedantic")
        else()
          set(_target_compile_flags "/W4")
        endif()
        string(REPLACE ";" " " _target_compile_flags "${_target_compile_flags}")
        set_target_properties(${target}__rosidl_typesupport_protobuf_c PROPERTIES
          COMPILE_FLAGS "${_target_compile_flags}"
        )

        target_include_directories(${target}__rosidl_typesupport_protobuf_c PUBLIC
          "$<BUILD_INTERFACE:${proto_ssot_output_dir}>"
          "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/proto2ros_ssot>"
          "$<BUILD_INTERFACE:${proto_ssot_output_dir}/msg>"
          "$<BUILD_INTERFACE:${_ts_c_output_path}>"
          "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/rosidl_typesupport_protobuf_c>"
          "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/rosidl_generator_cpp>"
          "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/rosidl_generator_c>"
          "$<INSTALL_INTERFACE:include/${PROJECT_NAME}>"
        )
        if(NOT WIN32)
          target_compile_options(${target}__rosidl_typesupport_protobuf_c PRIVATE "-include${rosidl_adapter_proto_VISIBILITY_CONTROL_HEADER}")
        else()
          target_compile_options(${target}__rosidl_typesupport_protobuf_c PRIVATE "/FI\"${rosidl_adapter_proto_VISIBILITY_CONTROL_HEADER}\"")
        endif()
        target_link_libraries(${target}__rosidl_typesupport_protobuf_c PUBLIC
          rmw::rmw
          rosidl_typesupport_interface::rosidl_typesupport_interface
          rosidl_typesupport_protobuf_c::rosidl_typesupport_protobuf_c
          ${target}__rosidl_generator_c
          ${Protobuf_LIBRARY}
          protobuf::libprotobuf
        )
        add_dependencies(${target}__rosidl_typesupport_protobuf_c ${target}_messages_gen)

        set_property(GLOBAL PROPERTY "rosidl_get_typesupport_target_${target}_rosidl_typesupport_protobuf_c" "${target}__rosidl_typesupport_protobuf_c")
        install(
          TARGETS ${target}__rosidl_typesupport_protobuf_c
          EXPORT export_${target}__rosidl_typesupport_protobuf_c
          ARCHIVE DESTINATION lib
          LIBRARY DESTINATION lib
          RUNTIME DESTINATION bin
        )
        install(
          DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/rosidl_typesupport_protobuf_c/"
          DESTINATION "include/${PROJECT_NAME}"
          PATTERN "*.cpp" EXCLUDE
          PATTERN "*.h"
          PATTERN "*.hpp"
        )
        ament_export_include_directories("include/${PROJECT_NAME}")
        rosidl_export_typesupport_libraries(__rosidl_typesupport_protobuf_c
          ${target}__rosidl_typesupport_protobuf_c)
        rosidl_export_typesupport_targets(__rosidl_typesupport_protobuf_c
          ${target}__rosidl_typesupport_protobuf_c)
        ament_export_targets(export_${target}__rosidl_typesupport_protobuf_c)
      endif()
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
    if(NOT TARGET ${target}__rosidl_typesupport_protobuf_cpp)
      install(
        DIRECTORY ${proto_ssot_output_dir}/msg/
        DESTINATION include/${PROJECT_NAME}/${target}/msg
        FILES_MATCHING PATTERN "*.h"
      )
    endif()
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
