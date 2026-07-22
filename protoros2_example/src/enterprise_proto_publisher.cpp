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

class EnterpriseProtoPublisherNode : public protoros2::EnterpriseNode
{
public:
  EnterpriseProtoPublisherNode() : EnterpriseNode("enterprise_proto_publisher_node"), count_(0)
  {
    publisher_ = this->create_proto_publisher<protoros2_example::msg::pb::ExampleMessage>("proto_msg_topic", 10);
    RCLCPP_INFO(
      this->get_logger(), "EnterpriseProtoPublisher created. Native Protobuf Fast-Path active: %s",
      publisher_->is_protobuf_native() ? "YES" : "NO");

    auto timer_callback = [this]() -> void {
      protoros2_example::msg::pb::ExampleMessage message;
      std::string text = "Hello from EnterpriseProtoPublisher! Count: " + std::to_string(count_++);
      message.set_message(text);

      RCLCPP_INFO(this->get_logger(), "Publishing via ProtoChannel: '%s'", text.c_str());
      this->publisher_->publish(message);
    };
    timer_ = this->create_wall_timer(std::chrono::seconds(1), timer_callback);
  }

private:
  rclcpp::TimerBase::SharedPtr timer_;
  protoros2::ProtoPublisher<protoros2_example::msg::pb::ExampleMessage>::SharedPtr publisher_;
  size_t count_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<EnterpriseProtoPublisherNode>());
  rclcpp::shutdown();
  return 0;
}
