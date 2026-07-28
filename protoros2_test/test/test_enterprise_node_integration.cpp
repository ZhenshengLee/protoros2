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
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <atomic>

#include "rclcpp/rclcpp.hpp"
#include "protoros2/enterprise_node.hpp"
#include "protoros2_test/msg/basic_types__typeadapter_protobuf_cpp.hpp"

class TestEnterpriseNodeIntegration : public ::testing::Test
{
protected:
  static void SetUpTestSuite() { rclcpp::init(0, nullptr); }

  static void TearDownTestSuite() { rclcpp::shutdown(); }

  void SetUp() override { node_ = std::make_shared<protoros2::EnterpriseNode>("test_enterprise_node_integration"); }

  void TearDown() override { node_.reset(); }

  std::shared_ptr<protoros2::EnterpriseNode> node_;
};

TEST_F(TestEnterpriseNodeIntegration, TestSmartRoutingStateAndAccessors)
{
  auto pub = node_->create_proto_publisher<protoros2_test::msg::pb::BasicTypes>("proto_routing_topic", 10);
  ASSERT_NE(pub, nullptr);
  EXPECT_NE(pub->get_raw_publisher(), nullptr);
  EXPECT_NE(pub->get_publisher_base(), nullptr);

  auto sub = node_->create_proto_subscription<protoros2_test::msg::pb::BasicTypes>(
    "proto_routing_topic", 10, [](const protoros2_test::msg::pb::BasicTypes &) {});
  ASSERT_NE(sub, nullptr);
  EXPECT_NE(sub->get_subscription_base(), nullptr);
  EXPECT_STREQ(sub->get_topic_name(), "/proto_routing_topic");

  const char * rmw_format = rmw_get_serialization_format();
  bool expected_protobuf_native = (rmw_format != nullptr && std::string(rmw_format) == "protobuf");
  EXPECT_EQ(pub->is_protobuf_native(), expected_protobuf_native);
  EXPECT_EQ(sub->is_protobuf_native(), expected_protobuf_native);
}

TEST_F(TestEnterpriseNodeIntegration, TestFlatChannelRoutingAndAccessors)
{
  auto pub = node_->create_flat_publisher<protoros2_test::msg::pb::BasicTypes>("flat_routing_topic", 10);
  ASSERT_NE(pub, nullptr);
  EXPECT_EQ(pub->get_raw_publisher(), nullptr);
  EXPECT_EQ(pub->get_publisher_base(), nullptr);
  EXPECT_NE(pub->get_iox_publisher(), nullptr);
  EXPECT_TRUE(pub->is_bypass_channel());

  auto sub = node_->create_flat_subscription<protoros2_test::msg::pb::BasicTypes>(
    "flat_routing_topic", 10, [](const protoros2_test::msg::pb::BasicTypes &) {});
  ASSERT_NE(sub, nullptr);
  EXPECT_EQ(sub->get_subscription_base(), nullptr);
  EXPECT_NE(sub->get_iox_subscriber(), nullptr);
  EXPECT_STREQ(sub->get_topic_name(), "/flat_routing_topic");
  EXPECT_TRUE(sub->is_bypass_channel());

  const char * rmw_format = rmw_get_serialization_format();
  bool expected_protobuf_native = (rmw_format != nullptr && std::string(rmw_format) == "protobuf");
  EXPECT_EQ(pub->is_protobuf_native(), expected_protobuf_native);
  EXPECT_EQ(sub->is_protobuf_native(), expected_protobuf_native);
}

TEST_F(TestEnterpriseNodeIntegration, TestRoundTripMessageDeliverySimpleCallback)
{
  std::atomic<bool> message_received{false};
  std::string received_text;
  int32_t received_int = 0;

  auto pub = node_->create_proto_publisher<protoros2_test::msg::pb::BasicTypes>("proto_roundtrip_simple", 10);

  auto sub = node_->create_proto_subscription<protoros2_test::msg::pb::BasicTypes>(
    "proto_roundtrip_simple", 10, [&](const protoros2_test::msg::pb::BasicTypes & msg) {
      received_text = msg.string_value();
      received_int = msg.int32_value();
      message_received = true;
    });

  protoros2_test::msg::pb::BasicTypes send_msg;
  send_msg.set_string_value("Hello from ProtoPublisher Fast-Path/Fallback!");
  send_msg.set_int32_value(2026);

  rclcpp::executors::SingleThreadedExecutor exec;
  exec.add_node(node_);

  auto start_time = std::chrono::steady_clock::now();
  while (!message_received && std::chrono::steady_clock::now() - start_time < std::chrono::seconds(3)) {
    pub->publish(send_msg);
    exec.spin_some();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  EXPECT_TRUE(message_received);
  EXPECT_EQ(received_text, "Hello from ProtoPublisher Fast-Path/Fallback!");
  EXPECT_EQ(received_int, 2026);
}

TEST_F(TestEnterpriseNodeIntegration, TestRoundTripMessageDeliveryWithInfoCallback)
{
  std::atomic<bool> message_received{false};
  std::string received_text;

  auto pub = node_->create_proto_publisher<protoros2_test::msg::pb::BasicTypes>("proto_roundtrip_info", 10);

  auto sub = node_->create_proto_subscription<protoros2_test::msg::pb::BasicTypes>(
    "proto_roundtrip_info", 10, [&](const protoros2_test::msg::pb::BasicTypes & msg, const rclcpp::MessageInfo & info) {
      (void)info;
      received_text = msg.string_value();
      message_received = true;
    });

  protoros2_test::msg::pb::BasicTypes send_msg;
  send_msg.set_string_value("With MessageInfo callback support");

  rclcpp::executors::SingleThreadedExecutor exec;
  exec.add_node(node_);

  auto start_time = std::chrono::steady_clock::now();
  while (!message_received && std::chrono::steady_clock::now() - start_time < std::chrono::seconds(3)) {
    pub->publish(send_msg);
    exec.spin_some();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  EXPECT_TRUE(message_received);
  EXPECT_EQ(received_text, "With MessageInfo callback support");
}

TEST_F(TestEnterpriseNodeIntegration, TestFlatChannelRoundTripMessageDelivery)
{
  std::atomic<bool> message_received{false};
  std::string received_text;
  int32_t received_int = 0;

  auto pub = node_->create_flat_publisher<protoros2_test::msg::pb::BasicTypes>("flat_roundtrip_topic", 10);

  auto sub = node_->create_flat_subscription<protoros2_test::msg::pb::BasicTypes>(
    "flat_roundtrip_topic", 10, [&](const protoros2_test::msg::pb::BasicTypes & msg) {
      received_text = msg.string_value();
      received_int = msg.int32_value();
      message_received = true;
    });

  protoros2_test::msg::pb::BasicTypes send_msg;
  send_msg.set_string_value("Hello from FlatPublisher Bypass Channel!");
  send_msg.set_int32_value(999);

  rclcpp::executors::SingleThreadedExecutor exec;
  exec.add_node(node_);

  auto start_time = std::chrono::steady_clock::now();
  while (!message_received && std::chrono::steady_clock::now() - start_time < std::chrono::seconds(3)) {
    pub->publish(send_msg);
    exec.spin_some();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  EXPECT_TRUE(message_received);
  EXPECT_EQ(received_text, "Hello from FlatPublisher Bypass Channel!");
  EXPECT_EQ(received_int, 999);
}

TEST_F(TestEnterpriseNodeIntegration, TestRoundTripIntraProcessZeroCopy)
{
  std::atomic<bool> message_received{false};
  std::uintptr_t sent_ptr = 0;
  std::uintptr_t received_ptr = 0;

  auto intra_node =
    std::make_shared<protoros2::EnterpriseNode>("test_intra", rclcpp::NodeOptions().use_intra_process_comms(true));

  auto pub = intra_node->create_proto_publisher<protoros2_test::msg::pb::BasicTypes>("proto_roundtrip_intra", 10);

  auto sub = intra_node->create_proto_subscription<protoros2_test::msg::pb::BasicTypes>(
    "proto_roundtrip_intra", 10, [&](std::unique_ptr<protoros2_test::msg::pb::BasicTypes> msg) {
      received_ptr = reinterpret_cast<std::uintptr_t>(msg.get());
      message_received = true;
    });

  auto send_msg = std::make_unique<protoros2_test::msg::pb::BasicTypes>();
  send_msg->set_string_value("Intra-process Zero-copy");
  sent_ptr = reinterpret_cast<std::uintptr_t>(send_msg.get());

  rclcpp::executors::SingleThreadedExecutor exec;
  exec.add_node(intra_node);

  pub->publish(std::move(send_msg));

  auto start_time = std::chrono::steady_clock::now();
  while (!message_received && std::chrono::steady_clock::now() - start_time < std::chrono::seconds(3)) {
    exec.spin_some();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  EXPECT_TRUE(message_received);
  EXPECT_EQ(sent_ptr, received_ptr);
}
