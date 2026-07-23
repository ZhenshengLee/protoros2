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
#include "protoros2/proto_polling_subscriber.hpp"
#include "protoros2_example/msg/example_message__typeadapter_protobuf_cpp.hpp"

using namespace std::chrono_literals;  // NOLINT(build/namespaces)

class EnterpriseProtoPollingSubscriberNode : public protoros2::EnterpriseNode
{
public:
  EnterpriseProtoPollingSubscriberNode() : EnterpriseNode("enterprise_proto_polling_subscriber_node")
  {
    // Create a ProtoPollingSubscriber conforming to Autoware's PollingSubscriber pattern
    polling_sub_ = protoros2::ProtoPollingSubscriber<protoros2_example::msg::pb::ExampleMessage>::create_subscription(
      this, "proto_msg_topic", rclcpp::QoS(10));

    // Main processing loop periodically polling the latest data using take_data()
    polling_timer_ = this->create_wall_timer(50ms, [this]() {
      auto latest_data = polling_sub_->take_data();
      if (latest_data) {
        auto ts = polling_sub_->last_taken_data_timestamp();
        RCLCPP_INFO(
          this->get_logger(), "Polled latest message via ProtoChannel: '%s', ts_valid: %d",
          latest_data->message().c_str(), ts.has_value());
      }
    });

    RCLCPP_INFO(this->get_logger(), "EnterpriseProtoPollingSubscriber created in Proto Polling mode.");
  }

private:
  protoros2::ProtoPollingSubscriber<protoros2_example::msg::pb::ExampleMessage>::SharedPtr polling_sub_;
  rclcpp::TimerBase::SharedPtr polling_timer_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<EnterpriseProtoPollingSubscriberNode>());
  rclcpp::shutdown();
  return 0;
}
