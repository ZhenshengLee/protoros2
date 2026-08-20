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
find_package(iceoryx2-cxx REQUIRED)
find_package(iceoryx2-c REQUIRED)

add_library(${PROJECT_NAME} SHARED
  src/flat_backend.cpp
)

target_include_directories(${PROJECT_NAME}
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)


target_link_libraries(${PROJECT_NAME}
  PUBLIC
    rclcpp::rclcpp
    iceoryx2-cxx::shared-lib-cxx
    iceoryx2-c::shared-lib
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
ament_export_dependencies(rclcpp rmw proto2ros rosidl_typesupport_protobuf_cpp iceoryx2-cxx iceoryx2-c)
ament_package(CONFIG_EXTRAS "cmake/protoros2-extras.cmake")
