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

#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "protoros2/enterprise_node.hpp"
#include "protoros2_example/msg/example_message__typeadapter_protobuf_cpp.hpp"

class EnterpriseFlatSubscriberNode : public protoros2::EnterpriseNode
{
public:
  EnterpriseFlatSubscriberNode() : EnterpriseNode("enterprise_flat_subscriber_node")
  {
    auto topic_callback = [this](const protoros2_example::msg::pb::ExampleMessage & msg) -> void {
      RCLCPP_INFO(this->get_logger(), "Received via FlatChannel: '%s'", msg.message().c_str());
    };

    subscription_ =
      this->create_flat_subscription<protoros2_example::msg::pb::ExampleMessage>("flat_msg_topic", 10, topic_callback);

    RCLCPP_INFO(
      this->get_logger(), "EnterpriseFlatSubscriber created. Bypass channel active: %s, Native Protobuf: %s",
      subscription_->is_bypass_channel() ? "YES" : "NO", subscription_->is_protobuf_native() ? "YES" : "NO");
  }

private:
  protoros2::FlatSubscription<protoros2_example::msg::pb::ExampleMessage>::SharedPtr subscription_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<EnterpriseFlatSubscriberNode>());
  rclcpp::shutdown();
  return 0;
}
