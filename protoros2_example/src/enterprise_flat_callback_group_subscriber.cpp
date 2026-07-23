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

#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "rclcpp/rclcpp.hpp"
#include "protoros2/enterprise_node.hpp"
#include "protoros2_example/msg/example_message__typeadapter_protobuf_cpp.hpp"

class EnterpriseFlatCallbackGroupSubscriberNode : public protoros2::EnterpriseNode
{
public:
  EnterpriseFlatCallbackGroupSubscriberNode() : EnterpriseNode("enterprise_flat_cbgroup_subscriber_node")
  {
    // 1. Create a MutuallyExclusive callback group for fast critical path
    fast_group_ = this->create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);
    rclcpp::SubscriptionOptions fast_opt;
    fast_opt.callback_group = fast_group_;

    sub_fast_ = this->create_flat_subscription<protoros2_example::msg::pb::ExampleMessage>(
      "flat_msg_topic", 10,
      [this](const protoros2_example::msg::pb::ExampleMessage & msg) {
        RCLCPP_INFO(
          this->get_logger(), "[Fast Group Thread %zu] Received: '%s'",
          std::hash<std::thread::id>{}(std::this_thread::get_id()), msg.message().c_str());
      },
      fast_opt);

    // 2. Create a Reentrant callback group for heavy processing path
    heavy_group_ = this->create_callback_group(rclcpp::CallbackGroupType::Reentrant);
    rclcpp::SubscriptionOptions heavy_opt;
    heavy_opt.callback_group = heavy_group_;

    sub_heavy_ = this->create_flat_subscription<protoros2_example::msg::pb::ExampleMessage>(
      "flat_msg_topic", 10,
      [this](const protoros2_example::msg::pb::ExampleMessage & msg) {
        RCLCPP_INFO(
          this->get_logger(), "[Heavy Group Thread %zu] Processing heavy message: '%s'",
          std::hash<std::thread::id>{}(std::this_thread::get_id()), msg.message().c_str());
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      },
      heavy_opt);

    RCLCPP_INFO(
      this->get_logger(), "EnterpriseFlatCallbackGroupSubscriber created with MutuallyExclusive and Reentrant groups.");
  }

private:
  rclcpp::CallbackGroup::SharedPtr fast_group_;
  rclcpp::CallbackGroup::SharedPtr heavy_group_;
  protoros2::FlatSubscription<protoros2_example::msg::pb::ExampleMessage>::SharedPtr sub_fast_;
  protoros2::FlatSubscription<protoros2_example::msg::pb::ExampleMessage>::SharedPtr sub_heavy_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::executors::MultiThreadedExecutor exec(rclcpp::ExecutorOptions(), 4);
  auto node = std::make_shared<EnterpriseFlatCallbackGroupSubscriberNode>();
  exec.add_node(node);
  exec.spin();
  rclcpp::shutdown();
  return 0;
}
