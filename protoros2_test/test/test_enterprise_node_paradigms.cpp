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
#include "protoros2/proto_polling_subscriber.hpp"
#include "protoros2_test/msg/basic_types__typeadapter_protobuf_cpp.hpp"

class TestEnterpriseNodeParadigms : public ::testing::Test
{
protected:
  static void SetUpTestSuite() { rclcpp::init(0, nullptr); }

  static void TearDownTestSuite() { rclcpp::shutdown(); }

  void SetUp() override { node_ = std::make_shared<protoros2::EnterpriseNode>("test_enterprise_paradigms_node"); }

  void TearDown() override { node_.reset(); }

  std::shared_ptr<protoros2::EnterpriseNode> node_;
};

TEST_F(TestEnterpriseNodeParadigms, TestFlatSubscriptionWaitableAndGuardCondition)
{
  auto sub = node_->create_flat_subscription<protoros2_test::msg::pb::BasicTypes>(
    "flat_gc_test", 10, [](const protoros2_test::msg::pb::BasicTypes &) {});
  ASSERT_NE(sub, nullptr);
  EXPECT_NE(sub->get_guard_condition(), nullptr);
  EXPECT_NE(sub->get_waitable(), nullptr);

  rclcpp::WaitSet wait_set;
  EXPECT_NO_THROW(wait_set.add_waitable(sub->get_waitable()));
}

TEST_F(TestEnterpriseNodeParadigms, TestFlatSubscriptionPullModeTake)
{
  auto pub = node_->create_flat_publisher<protoros2_test::msg::pb::BasicTypes>("flat_pull_test", 10);
  ASSERT_NE(pub, nullptr);

  auto sub = node_->create_flat_subscription<protoros2_test::msg::pb::BasicTypes>(
    "flat_pull_test", 10, protoros2::FlatSubscriptionMode::WaitSetPull);
  ASSERT_NE(sub, nullptr);

  protoros2_test::msg::pb::BasicTypes send_msg;
  send_msg.set_string_value("Flat pull mode test");
  send_msg.set_int32_value(1001);

  pub->publish(send_msg);

  protoros2_test::msg::pb::BasicTypes taken_msg;
  bool received = false;
  auto start_time = std::chrono::steady_clock::now();
  while (!received && std::chrono::steady_clock::now() - start_time < std::chrono::seconds(2)) {
    if (sub->take(taken_msg)) {
      received = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }

  EXPECT_TRUE(received);
  EXPECT_EQ(taken_msg.string_value(), "Flat pull mode test");
  EXPECT_EQ(taken_msg.int32_value(), 1001);
}

TEST_F(TestEnterpriseNodeParadigms, TestProtoSubscriptionTake)
{
  auto pub = node_->create_proto_publisher<protoros2_test::msg::pb::BasicTypes>("proto_take_test", 10);
  ASSERT_NE(pub, nullptr);

  auto noexec_cb_group = node_->create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive, false);
  rclcpp::SubscriptionOptions options;
  options.callback_group = noexec_cb_group;

  auto sub = node_->create_proto_subscription<protoros2_test::msg::pb::BasicTypes>(
    "proto_take_test", 10, [](const protoros2_test::msg::pb::BasicTypes &) {}, options);
  ASSERT_NE(sub, nullptr);

  protoros2_test::msg::pb::BasicTypes send_msg;
  send_msg.set_string_value("Proto take test");
  send_msg.set_int32_value(2026);

  rclcpp::executors::SingleThreadedExecutor exec;
  exec.add_node(node_);

  auto start_time = std::chrono::steady_clock::now();
  bool received = false;
  protoros2_test::msg::pb::BasicTypes taken_msg;

  while (!received && std::chrono::steady_clock::now() - start_time < std::chrono::seconds(2)) {
    pub->publish(send_msg);
    exec.spin_some();
    if (sub->take(taken_msg)) {
      received = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }

  EXPECT_TRUE(received);
  EXPECT_EQ(taken_msg.string_value(), "Proto take test");
  EXPECT_EQ(taken_msg.int32_value(), 2026);
}

TEST_F(TestEnterpriseNodeParadigms, TestFlatSubscriptionCallbackGroupRegistration)
{
  auto cb_group = node_->create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);
  rclcpp::SubscriptionOptions sub_opt;
  sub_opt.callback_group = cb_group;

  std::atomic<bool> called{false};
  auto sub = node_->create_flat_subscription<protoros2_test::msg::pb::BasicTypes>(
    "flat_cbgroup_test", 10,
    [&](const protoros2_test::msg::pb::BasicTypes & msg) {
      EXPECT_EQ(msg.string_value(), "Callback group test");
      called = true;
    },
    sub_opt);

  ASSERT_NE(sub, nullptr);
  EXPECT_NE(sub->get_waitable(), nullptr);

  auto pub = node_->create_flat_publisher<protoros2_test::msg::pb::BasicTypes>("flat_cbgroup_test", 10);
  protoros2_test::msg::pb::BasicTypes send_msg;
  send_msg.set_string_value("Callback group test");

  rclcpp::executors::SingleThreadedExecutor exec;
  exec.add_node(node_);

  auto start_time = std::chrono::steady_clock::now();
  while (!called && std::chrono::steady_clock::now() - start_time < std::chrono::seconds(2)) {
    pub->publish(send_msg);
    exec.spin_some();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }

  EXPECT_TRUE(called);
}

TEST_F(TestEnterpriseNodeParadigms, TestProtoPollingSubscriber)
{
  auto pub = node_->create_proto_publisher<protoros2_test::msg::pb::BasicTypes>("proto_polling_test", 10);
  ASSERT_NE(pub, nullptr);

  auto sub = protoros2::ProtoPollingSubscriber<protoros2_test::msg::pb::BasicTypes>::create_subscription(
    node_.get(), "proto_polling_test", rclcpp::QoS(10));
  ASSERT_NE(sub, nullptr);

  protoros2_test::msg::pb::BasicTypes send_msg;
  send_msg.set_string_value("Polling test");
  send_msg.set_int32_value(404);

  rclcpp::executors::SingleThreadedExecutor exec;
  exec.add_node(node_);

  auto start_time = std::chrono::steady_clock::now();
  bool received = false;

  while (!received && std::chrono::steady_clock::now() - start_time < std::chrono::seconds(2)) {
    pub->publish(send_msg);
    exec.spin_some();
    auto latest_data = sub->take_data();
    if (latest_data) {
      EXPECT_EQ(latest_data->string_value(), "Polling test");
      EXPECT_EQ(latest_data->int32_value(), 404);
      received = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }

  EXPECT_TRUE(received);
}
