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

class EnterpriseFlatWaitsetSubscriberNode : public protoros2::EnterpriseNode
{
public:
  EnterpriseFlatWaitsetSubscriberNode() : EnterpriseNode("enterprise_flat_waitset_subscriber_node")
  {
    // Create a FlatSubscription in WaitSet / Pull mode (no callback passed)
    flat_sub_ = this->create_flat_subscription<protoros2_example::msg::pb::ExampleMessage>(
      "flat_msg_topic", 10, protoros2::FlatSubscriptionMode::WaitSetPull);

    // Create a timer to periodically run waitset loop and check for messages
    timer_ = this->create_wall_timer(100ms, [this]() {
      rclcpp::WaitSet wait_set;
      if (flat_sub_->get_waitable()) {
        wait_set.add_waitable(flat_sub_->get_waitable());
      } else if (flat_sub_->get_guard_condition()) {
        wait_set.add_guard_condition(flat_sub_->get_guard_condition());
      }

      auto result = wait_set.wait(50ms);
      if (result.kind() == rclcpp::WaitResultKind::Ready) {
        protoros2_example::msg::pb::ExampleMessage msg;
        while (flat_sub_->take(msg)) {
          RCLCPP_INFO(this->get_logger(), "WaitSet woke up! Taken message: '%s'", msg.message().c_str());
        }
      }

      // WaitSet destructor automatically handles the in_use_by_wait_set_state cleanup.
    });

    RCLCPP_INFO(this->get_logger(), "EnterpriseFlatWaitsetSubscriber created and listening via WaitSet.");
  }

private:
  protoros2::FlatSubscription<protoros2_example::msg::pb::ExampleMessage>::SharedPtr flat_sub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<EnterpriseFlatWaitsetSubscriberNode>());
  rclcpp::shutdown();
  return 0;
}
