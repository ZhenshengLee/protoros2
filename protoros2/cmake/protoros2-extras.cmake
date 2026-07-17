# Copyright 2026 the protoros2 authors
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

# generated from protoros2/cmake/protoros2-extras.cmake

find_package(proto2ros REQUIRED)
find_package(rosidl_typesupport_protobuf_cpp REQUIRED)

include("${protoros2_DIR}/proto2ros_wrapper.cmake")
