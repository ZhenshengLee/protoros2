cmake_minimum_required(VERSION 3.20)
project(protoros2)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rmw REQUIRED)
find_package(iceoryx_posh REQUIRED)
find_package(iceoryx_hoofs REQUIRED)
find_package(flatbuffers REQUIRED)

add_library(${PROJECT_NAME} SHARED
  src/flat_backend.cpp
  src/posh_runtime_helper.cpp
)

target_include_directories(${PROJECT_NAME}
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${iceoryx_posh_INCLUDE_DIRS}
    ${iceoryx_hoofs_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME}
  PUBLIC
    rclcpp::rclcpp
  PRIVATE
    iceoryx_posh::iceoryx_posh
    iceoryx_hoofs::iceoryx_hoofs
    flatbuffers::flatbuffers
)

install(
  TARGETS ${PROJECT_NAME}
  EXPORT ${PROJECT_NAME}Targets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin
  INCLUDES DESTINATION include
)

install(
  DIRECTORY cmake
  DESTINATION share/${PROJECT_NAME}
)

install(
  DIRECTORY include/
  DESTINATION include
)

ament_export_targets(${PROJECT_NAME}Targets HAS_LIBRARY_TARGET)
ament_export_include_directories(include)
ament_export_dependencies(rclcpp rmw proto2ros rosidl_typesupport_protobuf_cpp)
ament_package(CONFIG_EXTRAS "cmake/protoros2-extras.cmake")
