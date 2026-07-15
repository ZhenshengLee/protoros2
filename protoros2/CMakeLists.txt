cmake_minimum_required(VERSION 3.20)
project(protoros2)

find_package(ament_cmake REQUIRED)

install(
  DIRECTORY cmake
  DESTINATION share/${PROJECT_NAME}
)

ament_export_dependencies(proto2ros rosidl_typesupport_protobuf_cpp)
ament_package(CONFIG_EXTRAS "cmake/protoros2-extras.cmake")
