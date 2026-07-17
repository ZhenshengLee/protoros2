// Copyright 2026 the protoros2 authors
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

// Dual-engine consistency test: verifies that proto2ros conversions and
// rosidl_typesupport_protobuf_cpp conversions produce identical output.

#include <gtest/gtest.h>
#include <cmath>
#include <string>
#include <vector>

#include "msg/NestedConsistencyMessage.pb.h"
#include "msg/TestConsistencyMessage.pb.h"
#include "protoros2_test/conversions.hpp"
#include "protoros2_test/msg/nested_consistency_message.hpp"
#include "protoros2_test/msg/nested_consistency_message__typeadapter_protobuf_cpp.hpp"
#include "protoros2_test/msg/test_consistency_message.hpp"
#include "protoros2_test/msg/test_consistency_message__typeadapter_protobuf_cpp.hpp"

namespace
{

bool AreDoublesNear(double a, double b, double eps = 1e-6) { return std::fabs(a - b) < eps; }

bool AreFloatsNear(float a, float b, float eps = 1e-5f) { return std::fabs(a - b) < eps; }

bool AreRosMessagesEqual(
  const protoros2_test::msg::TestConsistencyMessage & lhs, const protoros2_test::msg::TestConsistencyMessage & rhs)
{
  if (
    lhs.flag_bool != rhs.flag_bool || lhs.val_int32 != rhs.val_int32 || lhs.val_int64 != rhs.val_int64 ||
    lhs.val_uint32 != rhs.val_uint32 || lhs.val_uint64 != rhs.val_uint64 ||
    !AreFloatsNear(lhs.val_float, rhs.val_float) || !AreDoublesNear(lhs.val_double, rhs.val_double) ||
    lhs.val_string != rhs.val_string || lhs.list_int32 != rhs.list_int32 || lhs.list_string != rhs.list_string) {
    return false;
  }

  if (
    lhs.nested_msg.id != rhs.nested_msg.id || lhs.nested_msg.label != rhs.nested_msg.label ||
    lhs.nested_msg.scores.size() != rhs.nested_msg.scores.size()) {
    return false;
  }
  for (size_t i = 0; i < lhs.nested_msg.scores.size(); ++i) {
    if (!AreDoublesNear(lhs.nested_msg.scores[i], rhs.nested_msg.scores[i])) {
      return false;
    }
  }

  if (lhs.list_nested.size() != rhs.list_nested.size()) {
    return false;
  }
  for (size_t i = 0; i < lhs.list_nested.size(); ++i) {
    if (
      lhs.list_nested[i].id != rhs.list_nested[i].id || lhs.list_nested[i].label != rhs.list_nested[i].label ||
      lhs.list_nested[i].scores.size() != rhs.list_nested[i].scores.size()) {
      return false;
    }
    for (size_t j = 0; j < lhs.list_nested[i].scores.size(); ++j) {
      if (!AreDoublesNear(lhs.list_nested[i].scores[j], rhs.list_nested[i].scores[j])) {
        return false;
      }
    }
  }

  return true;
}

}  // namespace

TEST(TypeMappingConsistencyTest, RosToProtoScalarAndVectorConsistency)
{
  protoros2_test::msg::TestConsistencyMessage ros_msg;
  ros_msg.flag_bool = true;
  ros_msg.val_int32 = -12345;
  ros_msg.val_int64 = -9876543210123LL;
  ros_msg.val_uint32 = 54321U;
  ros_msg.val_uint64 = 123456789012345ULL;
  ros_msg.val_float = 3.14159f;
  ros_msg.val_double = 2.718281828459045;
  ros_msg.val_string = "hello_protoros2_ssot";
  ros_msg.list_int32 = {1, -2, 3, -4, 5};
  ros_msg.list_string = {"alpha", "beta", "gamma"};

  ros_msg.nested_msg.id = 100;
  ros_msg.nested_msg.label = "nested_label";
  ros_msg.nested_msg.scores = {0.1, 0.2, 0.3};

  protoros2_test::msg::NestedConsistencyMessage item1;
  item1.id = 201;
  item1.label = "item_1";
  item1.scores = {1.1, 2.2};
  ros_msg.list_nested.push_back(item1);

  protoros2_test::msg::NestedConsistencyMessage item2;
  item2.id = 202;
  item2.label = "item_2";
  item2.scores = {3.3, 4.4, 5.5};
  ros_msg.list_nested.push_back(item2);

  // Convert via proto2ros conversion bridge
  protoros2_test::msg::pb::TestConsistencyMessage pb_via_proto2ros;
  protoros2_test::conversions::Convert(ros_msg, &pb_via_proto2ros);

  // Convert via rosidl_typesupport_protobuf_cpp
  protoros2_test::msg::pb::TestConsistencyMessage pb_via_typesupport;
  protoros2_test::msg::typesupport_protobuf_cpp::convert_to_proto(ros_msg, pb_via_typesupport);

  // Verify that both conversion engines produce exactly identical serialized protobuf strings
  std::string serialized_proto2ros;
  std::string serialized_typesupport;
  ASSERT_TRUE(pb_via_proto2ros.SerializeToString(&serialized_proto2ros));
  ASSERT_TRUE(pb_via_typesupport.SerializeToString(&serialized_typesupport));

  EXPECT_EQ(serialized_proto2ros, serialized_typesupport);
}

TEST(TypeMappingConsistencyTest, ProtoToRosScalarAndVectorConsistency)
{
  protoros2_test::msg::pb::TestConsistencyMessage pb_msg;
  pb_msg.set_flag_bool(true);
  pb_msg.set_val_int32(-999);
  pb_msg.set_val_int64(-88888888888LL);
  pb_msg.set_val_uint32(777U);
  pb_msg.set_val_uint64(66666666666ULL);
  pb_msg.set_val_float(1.234f);
  pb_msg.set_val_double(9.87654321);
  pb_msg.set_val_string("protobuf_direct_string");
  pb_msg.add_list_int32(10);
  pb_msg.add_list_int32(20);
  pb_msg.add_list_int32(30);
  pb_msg.add_list_string("first");
  pb_msg.add_list_string("second");

  auto * nested = pb_msg.mutable_nested_msg();
  nested->set_id(50);
  nested->set_label("nested_proto");
  nested->add_scores(0.99);
  nested->add_scores(0.88);

  auto * list_item = pb_msg.add_list_nested();
  list_item->set_id(60);
  list_item->set_label("list_nested_proto");
  list_item->add_scores(7.77);

  // Convert via proto2ros conversion bridge
  protoros2_test::msg::TestConsistencyMessage ros_via_proto2ros;
  protoros2_test::conversions::Convert(pb_msg, &ros_via_proto2ros);

  // Convert via rosidl_typesupport_protobuf_cpp
  protoros2_test::msg::TestConsistencyMessage ros_via_typesupport;
  protoros2_test::msg::typesupport_protobuf_cpp::convert_to_ros(pb_msg, ros_via_typesupport);

  // Verify that both converted ROS messages are equal
  EXPECT_TRUE(AreRosMessagesEqual(ros_via_proto2ros, ros_via_typesupport));
}

TEST(TypeMappingConsistencyTest, TypeAdapterProtobufCppBridgeConsistency)
{
  protoros2_test::msg::TestConsistencyMessage ros_original;
  ros_original.flag_bool = false;
  ros_original.val_int32 = 42;
  ros_original.val_string = "type_adapter_test";
  ros_original.list_int32 = {100, 200};

  using Adapter =
    rclcpp::TypeAdapter<protoros2_test::msg::pb::TestConsistencyMessage, protoros2_test::msg::TestConsistencyMessage>;

  // Test convert_to_custom (ros -> proto) via TypeAdapter
  protoros2_test::msg::pb::TestConsistencyMessage custom_proto;
  Adapter::convert_to_custom(ros_original, custom_proto);

  EXPECT_EQ(custom_proto.flag_bool(), ros_original.flag_bool);
  EXPECT_EQ(custom_proto.val_int32(), ros_original.val_int32);
  EXPECT_EQ(custom_proto.val_string(), ros_original.val_string);
  ASSERT_EQ(custom_proto.list_int32_size(), 2);
  EXPECT_EQ(custom_proto.list_int32(0), 100);
  EXPECT_EQ(custom_proto.list_int32(1), 200);

  // Test convert_to_ros_message (proto -> ros) via TypeAdapter
  protoros2_test::msg::TestConsistencyMessage ros_restored;
  Adapter::convert_to_ros_message(custom_proto, ros_restored);

  EXPECT_TRUE(AreRosMessagesEqual(ros_original, ros_restored));
}
