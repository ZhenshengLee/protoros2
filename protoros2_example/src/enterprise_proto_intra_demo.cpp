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
#include <cinttypes>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>

#include "rclcpp/rclcpp.hpp"
#include "protoros2/enterprise_node.hpp"
#include "protoros2_example/msg/compressed_image__typeadapter_protobuf_cpp.hpp"

struct Producer : public protoros2::EnterpriseNode
{
  Producer(const std::string & name, const std::string & output)
  : EnterpriseNode(name, rclcpp::NodeOptions().use_intra_process_comms(true))
  {
    // Create a publisher on the output topic using ProtoPublisher.
    pub_ = this->create_proto_publisher<protoros2_example::msg::pb::CompressedImage>(output, 10);

    auto callback = [this]() -> void {
      static int count = 0;
      auto msg = std::make_unique<protoros2_example::msg::pb::CompressedImage>();

      auto now_ns = std::chrono::high_resolution_clock::now().time_since_epoch();
      auto sec_part = std::chrono::duration_cast<std::chrono::seconds>(now_ns);
      auto nsec_part = std::chrono::duration_cast<std::chrono::nanoseconds>(now_ns - sec_part);
      msg->mutable_timestamp()->set_seconds(sec_part.count());
      msg->mutable_timestamp()->set_nanos(nsec_part.count());

      msg->set_frame_id("camera_intra");
      msg->set_format("jpeg");
      msg->set_data("dummy_data_intra_process_" + std::to_string(count));

      RCLCPP_INFO(
        this->get_logger(), "Published CompressedImage #%d with format: %s, and address: 0x%" PRIXPTR, count++,
        msg->format().c_str(), reinterpret_cast<std::uintptr_t>(msg.get()));

      pub_->publish(std::move(msg));
    };

    timer_ = this->create_wall_timer(std::chrono::seconds(1), callback);
  }

  protoros2::ProtoPublisher<protoros2_example::msg::pb::CompressedImage>::SharedPtr pub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

struct Consumer : public protoros2::EnterpriseNode
{
  Consumer(const std::string & name, const std::string & input)
  : EnterpriseNode(name, rclcpp::NodeOptions().use_intra_process_comms(true))
  {
    // Create a subscription using unique_ptr callback to observe zero-copy.
    sub_ = this->create_proto_subscription<protoros2_example::msg::pb::CompressedImage>(
      input, 10, [this](std::unique_ptr<protoros2_example::msg::pb::CompressedImage> msg) {
        RCLCPP_INFO(
          this->get_logger(), "Received CompressedImage with format: %s, data: %s, and address: 0x%" PRIXPTR,
          msg->format().c_str(), msg->data().c_str(), reinterpret_cast<std::uintptr_t>(msg.get()));
      });
  }

  protoros2::ProtoSubscription<protoros2_example::msg::pb::CompressedImage>::SharedPtr sub_;
};

int main(int argc, char * argv[])
{
  setvbuf(stdout, NULL, _IONBF, BUFSIZ);
  rclcpp::init(argc, argv);
  rclcpp::executors::SingleThreadedExecutor executor;

  auto producer = std::make_shared<Producer>("producer", "intra_image");
  auto consumer = std::make_shared<Consumer>("consumer", "intra_image");

  executor.add_node(producer);
  executor.add_node(consumer);
  executor.spin();

  rclcpp::shutdown();
  return 0;
}
