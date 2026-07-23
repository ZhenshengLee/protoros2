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

#include "rclcpp/rclcpp.hpp"
#include "protoros2/enterprise_node.hpp"
#include "protoros2_example/msg/example_message__typeadapter_protobuf_cpp.hpp"

using namespace std::chrono_literals;  // NOLINT(build/namespaces)

class EnterpriseFlatPollingSubscriberNode : public protoros2::EnterpriseNode
{
public:
  EnterpriseFlatPollingSubscriberNode() : EnterpriseNode("enterprise_flat_polling_subscriber_node")
  {
    // Create a PollingSubscription (Pull Mode) with MutuallyExclusive callback group option
    auto cb_group = this->create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);
    rclcpp::SubscriptionOptions sub_opt;
    sub_opt.callback_group = cb_group;

    polling_sub_ = this->create_flat_subscription<protoros2_example::msg::pb::ExampleMessage>(
      "flat_msg_topic", 10, protoros2::FlatSubscriptionMode::WaitSetPull, sub_opt);

    // Main processing loop periodically polling the latest data using the new wait_for_data API
    polling_timer_ = this->create_wall_timer(50ms, [this]() {
      protoros2_example::msg::pb::ExampleMessage latest_msg;

      // Black-box WaitSet usage hidden in wait_for_data
      if (polling_sub_->wait_for_data(10ms)) {
        bool received_any = false;
        while (polling_sub_->take(latest_msg)) {
          received_any = true;
        }
        if (received_any) {
          RCLCPP_INFO(this->get_logger(), "Polled latest message via FlatChannel: '%s'", latest_msg.message().c_str());
        }
      }
    });

    RCLCPP_INFO(this->get_logger(), "EnterpriseFlatPollingSubscriber created in pure Pull/Polling mode.");
  }

private:
  protoros2::FlatSubscription<protoros2_example::msg::pb::ExampleMessage>::SharedPtr polling_sub_;
  rclcpp::TimerBase::SharedPtr polling_timer_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<EnterpriseFlatPollingSubscriberNode>());
  rclcpp::shutdown();
  return 0;
}
