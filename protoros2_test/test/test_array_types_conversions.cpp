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
#include <google/protobuf/util/message_differencer.h>
#include <string>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "protoros2_test/msg/array_types__typeadapter_protobuf_cpp.hpp"
#include "protoros2_test/conversions.hpp"

using ArrayTypesProto = protoros2_test::msg::pb::ArrayTypes;
using ArrayTypesRos = protoros2_test::msg::ArrayTypes;
using ArrayTypesAdapter = rclcpp::TypeAdapter<ArrayTypesProto, ArrayTypesRos>;

class ArrayTypesConversionTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    pb_orig_.add_int32_values(-100);
    pb_orig_.add_int32_values(0);
    pb_orig_.add_int32_values(100);
    pb_orig_.add_int32_values(999999);

    pb_orig_.add_double_values(1.1);
    pb_orig_.add_double_values(-2.22222);
    pb_orig_.add_double_values(0.0);

    pb_orig_.add_string_values("Alpha");
    pb_orig_.add_string_values("Beta");
    pb_orig_.add_string_values("Gamma with space and utf8 🚗");

    pb_orig_.add_bool_values(true);
    pb_orig_.add_bool_values(false);
    pb_orig_.add_bool_values(true);
  }

  ArrayTypesProto pb_orig_;
};

TEST_F(ArrayTypesConversionTest, Roundtrip_TypeAdapter)
{
  ArrayTypesRos ros_msg;
  ArrayTypesAdapter::convert_to_ros_message(pb_orig_, ros_msg);

  ASSERT_EQ(ros_msg.int32_values.size(), static_cast<size_t>(pb_orig_.int32_values_size()));
  for (int i = 0; i < pb_orig_.int32_values_size(); ++i) {
    EXPECT_EQ(ros_msg.int32_values[i], pb_orig_.int32_values(i));
  }

  ASSERT_EQ(ros_msg.double_values.size(), static_cast<size_t>(pb_orig_.double_values_size()));
  for (int i = 0; i < pb_orig_.double_values_size(); ++i) {
    EXPECT_DOUBLE_EQ(ros_msg.double_values[i], pb_orig_.double_values(i));
  }

  ASSERT_EQ(ros_msg.string_values.size(), static_cast<size_t>(pb_orig_.string_values_size()));
  for (int i = 0; i < pb_orig_.string_values_size(); ++i) {
    EXPECT_EQ(ros_msg.string_values[i], pb_orig_.string_values(i));
  }

  ASSERT_EQ(ros_msg.bool_values.size(), static_cast<size_t>(pb_orig_.bool_values_size()));
  for (int i = 0; i < pb_orig_.bool_values_size(); ++i) {
    EXPECT_EQ(ros_msg.bool_values[i], pb_orig_.bool_values(i));
  }

  ArrayTypesProto pb_converted;
  ArrayTypesAdapter::convert_to_custom(ros_msg, pb_converted);

  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(pb_orig_, pb_converted));
}

TEST_F(ArrayTypesConversionTest, Roundtrip_Proto2Ros)
{
  ArrayTypesRos ros_msg = protoros2_test::conversions::Convert(pb_orig_);

  ASSERT_EQ(ros_msg.int32_values.size(), static_cast<size_t>(pb_orig_.int32_values_size()));
  for (int i = 0; i < pb_orig_.int32_values_size(); ++i) {
    EXPECT_EQ(ros_msg.int32_values[i], pb_orig_.int32_values(i));
  }

  ASSERT_EQ(ros_msg.double_values.size(), static_cast<size_t>(pb_orig_.double_values_size()));
  for (int i = 0; i < pb_orig_.double_values_size(); ++i) {
    EXPECT_DOUBLE_EQ(ros_msg.double_values[i], pb_orig_.double_values(i));
  }

  ASSERT_EQ(ros_msg.string_values.size(), static_cast<size_t>(pb_orig_.string_values_size()));
  for (int i = 0; i < pb_orig_.string_values_size(); ++i) {
    EXPECT_EQ(ros_msg.string_values[i], pb_orig_.string_values(i));
  }

  ASSERT_EQ(ros_msg.bool_values.size(), static_cast<size_t>(pb_orig_.bool_values_size()));
  for (int i = 0; i < pb_orig_.bool_values_size(); ++i) {
    EXPECT_EQ(ros_msg.bool_values[i], pb_orig_.bool_values(i));
  }

  ArrayTypesProto pb_converted = protoros2_test::conversions::Convert(ros_msg);

  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(pb_orig_, pb_converted));
}

TEST_F(ArrayTypesConversionTest, VerifyEngineEquivalence)
{
  ArrayTypesRos ros_from_adapter;
  ArrayTypesAdapter::convert_to_ros_message(pb_orig_, ros_from_adapter);

  ArrayTypesRos ros_from_proto2ros = protoros2_test::conversions::Convert(pb_orig_);

  EXPECT_EQ(ros_from_adapter.int32_values, ros_from_proto2ros.int32_values);
  EXPECT_EQ(ros_from_adapter.double_values, ros_from_proto2ros.double_values);
  EXPECT_EQ(ros_from_adapter.string_values, ros_from_proto2ros.string_values);
  EXPECT_EQ(ros_from_adapter.bool_values, ros_from_proto2ros.bool_values);

  ArrayTypesProto pb_back_adapter;
  ArrayTypesAdapter::convert_to_custom(ros_from_adapter, pb_back_adapter);

  ArrayTypesProto pb_back_proto2ros = protoros2_test::conversions::Convert(ros_from_proto2ros);

  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(pb_back_adapter, pb_back_proto2ros));
}

TEST_F(ArrayTypesConversionTest, EmptyArraysBehavior)
{
  ArrayTypesProto pb_empty;

  ArrayTypesRos ros_adapter;
  ArrayTypesAdapter::convert_to_ros_message(pb_empty, ros_adapter);

  ArrayTypesRos ros_proto2ros = protoros2_test::conversions::Convert(pb_empty);

  EXPECT_TRUE(ros_adapter.int32_values.empty());
  EXPECT_TRUE(ros_adapter.double_values.empty());
  EXPECT_TRUE(ros_adapter.string_values.empty());
  EXPECT_TRUE(ros_adapter.bool_values.empty());

  EXPECT_EQ(ros_adapter.int32_values, ros_proto2ros.int32_values);
  EXPECT_EQ(ros_adapter.double_values, ros_proto2ros.double_values);
  EXPECT_EQ(ros_adapter.string_values, ros_proto2ros.string_values);
  EXPECT_EQ(ros_adapter.bool_values, ros_proto2ros.bool_values);

  ArrayTypesProto pb_adapter_back;
  ArrayTypesAdapter::convert_to_custom(ros_adapter, pb_adapter_back);

  ArrayTypesProto pb_proto2ros_back = protoros2_test::conversions::Convert(ros_proto2ros);

  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(pb_empty, pb_adapter_back));
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(pb_empty, pb_proto2ros_back));
}
