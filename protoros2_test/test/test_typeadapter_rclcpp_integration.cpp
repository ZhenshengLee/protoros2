// Copyright 2026 The Garcia Team
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gtest/gtest.h>
#include <type_traits>

#include "rclcpp/rclcpp.hpp"
#include "protoros2_test/msg/basic_types__typeadapter_protobuf_cpp.hpp"
#include "protoros2_test/msg/array_types__typeadapter_protobuf_cpp.hpp"
#include "protoros2_test/msg/nested_message__typeadapter_protobuf_cpp.hpp"

using BasicTypesProto = protoros2_test::msg::pb::BasicTypes;
using BasicTypesRos = protoros2_test::msg::BasicTypes;
using BasicTypesAdapter = rclcpp::TypeAdapter<BasicTypesProto, BasicTypesRos>;

using ArrayTypesProto = protoros2_test::msg::pb::ArrayTypes;
using ArrayTypesRos = protoros2_test::msg::ArrayTypes;
using ArrayTypesAdapter = rclcpp::TypeAdapter<ArrayTypesProto, ArrayTypesRos>;

using NestedMessageProto = protoros2_test::msg::pb::NestedMessage;
using NestedMessageRos = protoros2_test::msg::NestedMessage;
using NestedMessageAdapter = rclcpp::TypeAdapter<NestedMessageProto, NestedMessageRos>;

TEST(TypeAdapterIntegrationTest, TypeTraitsVerification)
{
  EXPECT_TRUE(BasicTypesAdapter::is_specialized::value);
  EXPECT_TRUE((std::is_same<BasicTypesAdapter::custom_type, BasicTypesProto>::value));
  EXPECT_TRUE((std::is_same<BasicTypesAdapter::ros_message_type, BasicTypesRos>::value));

  EXPECT_TRUE(ArrayTypesAdapter::is_specialized::value);
  EXPECT_TRUE((std::is_same<ArrayTypesAdapter::custom_type, ArrayTypesProto>::value));
  EXPECT_TRUE((std::is_same<ArrayTypesAdapter::ros_message_type, ArrayTypesRos>::value));

  EXPECT_TRUE(NestedMessageAdapter::is_specialized::value);
  EXPECT_TRUE((std::is_same<NestedMessageAdapter::custom_type, NestedMessageProto>::value));
  EXPECT_TRUE((std::is_same<NestedMessageAdapter::ros_message_type, NestedMessageRos>::value));
}
