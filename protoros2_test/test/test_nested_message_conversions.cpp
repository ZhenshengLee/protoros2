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
#include "protoros2_test/msg/nested_message__typeadapter_protobuf_cpp.hpp"
#include "protoros2_test/conversions.hpp"

using NestedMessageProto = protoros2_test::msg::pb::NestedMessage;
using NestedMessageRos = protoros2_test::msg::NestedMessage;
using NestedMessageAdapter = rclcpp::TypeAdapter<NestedMessageProto, NestedMessageRos>;

class NestedMessageConversionTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    pb_orig_.set_header_id("header_nested_12345");

    auto * basic = pb_orig_.mutable_basic_info();
    basic->set_double_value(100.001);
    basic->set_string_value("embedded basic string");

    auto * arrays = pb_orig_.mutable_sensor_data();
    arrays->add_int32_values(10);
    arrays->add_int32_values(20);
    arrays->add_string_values("sensor_a");
    arrays->add_string_values("sensor_b");

    auto * main_sub = pb_orig_.mutable_main_sub();
    main_sub->set_name("MainSubModule");
    main_sub->set_id(777);
    main_sub->set_score(99.9);

    for (int i = 1; i <= 3; ++i) {
      auto * sub = pb_orig_.add_sub_list();
      sub->set_name("SubElement_" + std::to_string(i));
      sub->set_id(i * 100);
      sub->set_score(12.34 * i);
    }
  }

  NestedMessageProto pb_orig_;
};

TEST_F(NestedMessageConversionTest, Roundtrip_TypeAdapter)
{
  NestedMessageRos ros_msg;
  NestedMessageAdapter::convert_to_ros_message(pb_orig_, ros_msg);

  EXPECT_EQ(ros_msg.header_id, pb_orig_.header_id());
  EXPECT_DOUBLE_EQ(ros_msg.basic_info.double_value, pb_orig_.basic_info().double_value());
  EXPECT_EQ(ros_msg.basic_info.string_value, pb_orig_.basic_info().string_value());
  EXPECT_EQ(ros_msg.main_sub.name, pb_orig_.main_sub().name());
  EXPECT_EQ(ros_msg.main_sub.id, pb_orig_.main_sub().id());
  EXPECT_DOUBLE_EQ(ros_msg.main_sub.score, pb_orig_.main_sub().score());

  ASSERT_EQ(ros_msg.sub_list.size(), static_cast<size_t>(pb_orig_.sub_list_size()));
  for (int i = 0; i < pb_orig_.sub_list_size(); ++i) {
    EXPECT_EQ(ros_msg.sub_list[i].name, pb_orig_.sub_list(i).name());
    EXPECT_EQ(ros_msg.sub_list[i].id, pb_orig_.sub_list(i).id());
    EXPECT_DOUBLE_EQ(ros_msg.sub_list[i].score, pb_orig_.sub_list(i).score());
  }

  NestedMessageProto pb_converted;
  NestedMessageAdapter::convert_to_custom(ros_msg, pb_converted);

  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(pb_orig_, pb_converted));
}

TEST_F(NestedMessageConversionTest, Roundtrip_Proto2Ros)
{
  NestedMessageRos ros_msg = protoros2_test::conversions::Convert(pb_orig_);

  EXPECT_EQ(ros_msg.header_id, pb_orig_.header_id());
  EXPECT_DOUBLE_EQ(ros_msg.basic_info.double_value, pb_orig_.basic_info().double_value());
  EXPECT_EQ(ros_msg.basic_info.string_value, pb_orig_.basic_info().string_value());
  EXPECT_EQ(ros_msg.main_sub.name, pb_orig_.main_sub().name());
  EXPECT_EQ(ros_msg.main_sub.id, pb_orig_.main_sub().id());
  EXPECT_DOUBLE_EQ(ros_msg.main_sub.score, pb_orig_.main_sub().score());

  ASSERT_EQ(ros_msg.sub_list.size(), static_cast<size_t>(pb_orig_.sub_list_size()));
  for (int i = 0; i < pb_orig_.sub_list_size(); ++i) {
    EXPECT_EQ(ros_msg.sub_list[i].name, pb_orig_.sub_list(i).name());
    EXPECT_EQ(ros_msg.sub_list[i].id, pb_orig_.sub_list(i).id());
    EXPECT_DOUBLE_EQ(ros_msg.sub_list[i].score, pb_orig_.sub_list(i).score());
  }

  NestedMessageProto pb_converted = protoros2_test::conversions::Convert(ros_msg);

  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(pb_orig_, pb_converted));
}

TEST_F(NestedMessageConversionTest, VerifyEngineEquivalence)
{
  NestedMessageRos ros_from_adapter;
  NestedMessageAdapter::convert_to_ros_message(pb_orig_, ros_from_adapter);

  NestedMessageRos ros_from_proto2ros = protoros2_test::conversions::Convert(pb_orig_);

  EXPECT_EQ(ros_from_adapter.header_id, ros_from_proto2ros.header_id);
  EXPECT_DOUBLE_EQ(ros_from_adapter.basic_info.double_value, ros_from_proto2ros.basic_info.double_value);
  EXPECT_EQ(ros_from_adapter.basic_info.string_value, ros_from_proto2ros.basic_info.string_value);
  EXPECT_EQ(ros_from_adapter.sensor_data.int32_values, ros_from_proto2ros.sensor_data.int32_values);
  EXPECT_EQ(ros_from_adapter.sensor_data.string_values, ros_from_proto2ros.sensor_data.string_values);
  EXPECT_EQ(ros_from_adapter.main_sub.name, ros_from_proto2ros.main_sub.name);
  EXPECT_EQ(ros_from_adapter.main_sub.id, ros_from_proto2ros.main_sub.id);

  ASSERT_EQ(ros_from_adapter.sub_list.size(), ros_from_proto2ros.sub_list.size());
  for (size_t i = 0; i < ros_from_adapter.sub_list.size(); ++i) {
    EXPECT_EQ(ros_from_adapter.sub_list[i].name, ros_from_proto2ros.sub_list[i].name);
    EXPECT_EQ(ros_from_adapter.sub_list[i].id, ros_from_proto2ros.sub_list[i].id);
    EXPECT_DOUBLE_EQ(ros_from_adapter.sub_list[i].score, ros_from_proto2ros.sub_list[i].score);
  }

  NestedMessageProto pb_back_adapter;
  NestedMessageAdapter::convert_to_custom(ros_from_adapter, pb_back_adapter);

  NestedMessageProto pb_back_proto2ros = protoros2_test::conversions::Convert(ros_from_proto2ros);

  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(pb_back_adapter, pb_back_proto2ros));
}

TEST_F(NestedMessageConversionTest, PartialUpdatesAndMutations)
{
  NestedMessageRos ros_msg = protoros2_test::conversions::Convert(pb_orig_);

  ros_msg.header_id = "header_updated_999";
  ros_msg.basic_info.double_value = 555.555;
  ros_msg.main_sub.name = "ModifiedSub";
  ros_msg.sub_list.push_back(protoros2_test::msg::SubMessage().set__name("AddedSub").set__id(9999));

  NestedMessageProto pb_from_ros = protoros2_test::conversions::Convert(ros_msg);

  EXPECT_EQ(pb_from_ros.header_id(), "header_updated_999");
  EXPECT_DOUBLE_EQ(pb_from_ros.basic_info().double_value(), 555.555);
  EXPECT_EQ(pb_from_ros.main_sub().name(), "ModifiedSub");
  ASSERT_EQ(pb_from_ros.sub_list_size(), 4);
  EXPECT_EQ(pb_from_ros.sub_list(3).name(), "AddedSub");
  EXPECT_EQ(pb_from_ros.sub_list(3).id(), 9999);
}
