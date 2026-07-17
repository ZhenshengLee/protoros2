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
#include <limits>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "protoros2_test/msg/basic_types__typeadapter_protobuf_cpp.hpp"
#include "protoros2_test/conversions.hpp"

using BasicTypesProto = protoros2_test::msg::pb::BasicTypes;
using BasicTypesRos = protoros2_test::msg::BasicTypes;
using BasicTypesAdapter = rclcpp::TypeAdapter<BasicTypesProto, BasicTypesRos>;

class BasicTypesConversionTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    pb_orig_.set_double_value(3.141592653589793);
    pb_orig_.set_float_value(2.71828f);
    pb_orig_.set_int32_value(-123456);
    pb_orig_.set_int64_value(-987654321012345678LL);
    pb_orig_.set_uint32_value(4000000000U);
    pb_orig_.set_uint64_value(18000000000000000000ULL);
    pb_orig_.set_bool_value(true);
    pb_orig_.set_string_value("Hello, dual engines SSOT verification for BasicTypes!");
  }

  BasicTypesProto pb_orig_;
};

TEST_F(BasicTypesConversionTest, Roundtrip_TypeAdapter)
{
  BasicTypesRos ros_msg;
  BasicTypesAdapter::convert_to_ros_message(pb_orig_, ros_msg);

  EXPECT_DOUBLE_EQ(ros_msg.double_value, pb_orig_.double_value());
  EXPECT_FLOAT_EQ(ros_msg.float_value, pb_orig_.float_value());
  EXPECT_EQ(ros_msg.int32_value, pb_orig_.int32_value());
  EXPECT_EQ(ros_msg.int64_value, pb_orig_.int64_value());
  EXPECT_EQ(ros_msg.uint32_value, pb_orig_.uint32_value());
  EXPECT_EQ(ros_msg.uint64_value, pb_orig_.uint64_value());
  EXPECT_EQ(ros_msg.bool_value, pb_orig_.bool_value());
  EXPECT_EQ(ros_msg.string_value, pb_orig_.string_value());

  BasicTypesProto pb_converted;
  BasicTypesAdapter::convert_to_custom(ros_msg, pb_converted);

  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(pb_orig_, pb_converted));
}

TEST_F(BasicTypesConversionTest, Roundtrip_Proto2Ros)
{
  BasicTypesRos ros_msg = protoros2_test::conversions::Convert(pb_orig_);

  EXPECT_DOUBLE_EQ(ros_msg.double_value, pb_orig_.double_value());
  EXPECT_FLOAT_EQ(ros_msg.float_value, pb_orig_.float_value());
  EXPECT_EQ(ros_msg.int32_value, pb_orig_.int32_value());
  EXPECT_EQ(ros_msg.int64_value, pb_orig_.int64_value());
  EXPECT_EQ(ros_msg.uint32_value, pb_orig_.uint32_value());
  EXPECT_EQ(ros_msg.uint64_value, pb_orig_.uint64_value());
  EXPECT_EQ(ros_msg.bool_value, pb_orig_.bool_value());
  EXPECT_EQ(ros_msg.string_value, pb_orig_.string_value());

  BasicTypesProto pb_converted = protoros2_test::conversions::Convert(ros_msg);

  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(pb_orig_, pb_converted));
}

TEST_F(BasicTypesConversionTest, VerifyEngineEquivalence)
{
  BasicTypesRos ros_from_adapter;
  BasicTypesAdapter::convert_to_ros_message(pb_orig_, ros_from_adapter);

  BasicTypesRos ros_from_proto2ros = protoros2_test::conversions::Convert(pb_orig_);

  EXPECT_DOUBLE_EQ(ros_from_adapter.double_value, ros_from_proto2ros.double_value);
  EXPECT_FLOAT_EQ(ros_from_adapter.float_value, ros_from_proto2ros.float_value);
  EXPECT_EQ(ros_from_adapter.int32_value, ros_from_proto2ros.int32_value);
  EXPECT_EQ(ros_from_adapter.int64_value, ros_from_proto2ros.int64_value);
  EXPECT_EQ(ros_from_adapter.uint32_value, ros_from_proto2ros.uint32_value);
  EXPECT_EQ(ros_from_adapter.uint64_value, ros_from_proto2ros.uint64_value);
  EXPECT_EQ(ros_from_adapter.bool_value, ros_from_proto2ros.bool_value);
  EXPECT_EQ(ros_from_adapter.string_value, ros_from_proto2ros.string_value);

  BasicTypesProto pb_back_adapter;
  BasicTypesAdapter::convert_to_custom(ros_from_adapter, pb_back_adapter);

  BasicTypesProto pb_back_proto2ros = protoros2_test::conversions::Convert(ros_from_proto2ros);

  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(pb_back_adapter, pb_back_proto2ros));
}

TEST_F(BasicTypesConversionTest, BoundaryValuesAndLimits)
{
  BasicTypesProto pb_bounds;
  pb_bounds.set_double_value(std::numeric_limits<double>::max());
  pb_bounds.set_float_value(std::numeric_limits<float>::lowest());
  pb_bounds.set_int32_value(std::numeric_limits<int32_t>::min());
  pb_bounds.set_int64_value(std::numeric_limits<int64_t>::max());
  pb_bounds.set_uint32_value(std::numeric_limits<uint32_t>::max());
  pb_bounds.set_uint64_value(std::numeric_limits<uint64_t>::max());
  pb_bounds.set_bool_value(false);
  pb_bounds.set_string_value("");

  BasicTypesRos ros_adapter;
  BasicTypesAdapter::convert_to_ros_message(pb_bounds, ros_adapter);

  BasicTypesRos ros_proto2ros = protoros2_test::conversions::Convert(pb_bounds);

  EXPECT_DOUBLE_EQ(ros_adapter.double_value, ros_proto2ros.double_value);
  EXPECT_FLOAT_EQ(ros_adapter.float_value, ros_proto2ros.float_value);
  EXPECT_EQ(ros_adapter.int32_value, ros_proto2ros.int32_value);
  EXPECT_EQ(ros_adapter.int64_value, ros_proto2ros.int64_value);
  EXPECT_EQ(ros_adapter.uint32_value, ros_proto2ros.uint32_value);
  EXPECT_EQ(ros_adapter.uint64_value, ros_proto2ros.uint64_value);
  EXPECT_EQ(ros_adapter.bool_value, ros_proto2ros.bool_value);
  EXPECT_EQ(ros_adapter.string_value, ros_proto2ros.string_value);

  BasicTypesProto pb_adapter_back;
  BasicTypesAdapter::convert_to_custom(ros_adapter, pb_adapter_back);

  BasicTypesProto pb_proto2ros_back = protoros2_test::conversions::Convert(ros_proto2ros);

  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(pb_bounds, pb_adapter_back));
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(pb_bounds, pb_proto2ros_back));
}
