cmake_minimum_required(VERSION 3.10)
project(rosbag2_cpp_protobuf_converter)

# Default to C++17
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()
if(NOT CMAKE_CXX_STANDARD_REQUIRED)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

find_package(ament_cmake REQUIRED)
find_package(pluginlib REQUIRED)
find_package(rosbag2_cpp REQUIRED)
find_package(rosbag2_storage REQUIRED)

add_library(${PROJECT_NAME} SHARED
  src/protobuf_converter.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
  include
)

target_link_libraries(${PROJECT_NAME} PUBLIC
  pluginlib::pluginlib
  rosbag2_cpp::rosbag2_cpp
  rosbag2_storage::rosbag2_storage
)

pluginlib_export_plugin_description_file(rosbag2_cpp plugin_description.xml)

install(TARGETS ${PROJECT_NAME}
  EXPORT ${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

ament_export_libraries(${PROJECT_NAME})
ament_export_targets(${PROJECT_NAME})

ament_package()
