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
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <turbojpeg.h>

#include "protoros2/enterprise_node.hpp"
#include "protoros2_example/msg/compressed_image__typeadapter_protobuf_cpp.hpp"
#include "rclcpp/rclcpp.hpp"

class EnterpriseImageListenerNode : public protoros2::EnterpriseNode
{
public:
  EnterpriseImageListenerNode() : EnterpriseNode("enterprise_image_listener_node"), count_(0)
  {
    this->declare_parameter<std::string>("topic_name", "enterprise_compressed_image");
    this->declare_parameter<std::string>("channel_type", "flat");
    this->declare_parameter<bool>("decode_and_verify", true);

    std::string topic_name = this->get_parameter("topic_name").as_string();
    std::string channel_type = this->get_parameter("channel_type").as_string();
    decode_and_verify_ = this->get_parameter("decode_and_verify").as_bool();

    auto callback = [this](const protoros2_example::msg::pb::CompressedImage & msg) -> void {
      auto callback_enter_ts = std::chrono::high_resolution_clock::now();

      double transport_and_sched_ms = 0.0;
      if (msg.has_timestamp() && msg.timestamp().seconds() > 0) {
        auto pub_ns =
          std::chrono::seconds(msg.timestamp().seconds()) + std::chrono::nanoseconds(msg.timestamp().nanos());
        auto enter_ns = callback_enter_ts.time_since_epoch();
        transport_and_sched_ms = std::chrono::duration<double, std::milli>(enter_ns - pub_ns).count();
      }

      int width = 0;
      int height = 0;
      int channels = 0;
      double decode_ms = 0.0;
      bool decode_ok = false;

      if (decode_and_verify_ && msg.format() == "jpeg" && !msg.data().empty()) {
        auto decode_start = std::chrono::high_resolution_clock::now();
        decode_ok = decode_jpeg_memory(
          reinterpret_cast<const uint8_t *>(msg.data().data()), msg.data().size(), width, height, channels);
        auto decode_end = std::chrono::high_resolution_clock::now();
        decode_ms = std::chrono::duration<double, std::milli>(decode_end - decode_start).count();
      }

      auto callback_end_ts = std::chrono::high_resolution_clock::now();
      double callback_total_ms = std::chrono::duration<double, std::milli>(callback_end_ts - callback_enter_ts).count();

      count_++;
      if (decode_ok) {
        RCLCPP_INFO_THROTTLE(
          this->get_logger(), *this->get_clock(), 2000,
          "Received frame #%zu [%s] (%d x %d, %d ch, %zu bytes) | Transport & Sched Latency: %.3f ms | TurboJPEG "
          "Decode: %.3f ms | Total Callback: %.3f ms",
          count_, msg.frame_id().c_str(), width, height, channels, msg.data().size(), transport_and_sched_ms, decode_ms,
          callback_total_ms);
      } else {
        RCLCPP_INFO_THROTTLE(
          this->get_logger(), *this->get_clock(), 2000,
          "Received frame #%zu [%s] (format: %s, %zu bytes) | Transport & Sched Latency: %.3f ms | Total Callback: "
          "%.3f ms",
          count_, msg.frame_id().c_str(), msg.format().c_str(), msg.data().size(), transport_and_sched_ms,
          callback_total_ms);
      }
    };

    if (channel_type == "proto") {
      proto_sub_ =
        this->create_proto_subscription<protoros2_example::msg::pb::CompressedImage>(topic_name, 10, callback);
      RCLCPP_INFO(
        this->get_logger(), "Initialized ProtoChannel (Native Protobuf) subscription on topic '%s'",
        topic_name.c_str());
    } else {
      flat_sub_ = this->create_flat_subscription<protoros2_example::msg::pb::CompressedImage>(topic_name, 10, callback);
      RCLCPP_INFO(
        this->get_logger(), "Initialized FlatChannel (Iceoryx Zero-Copy Bypass) subscription on topic '%s'",
        topic_name.c_str());
    }
  }

private:
  bool decode_jpeg_memory(const uint8_t * jpeg_data, size_t jpeg_size, int & width, int & height, int & channels)
  {
    if (!jpeg_data || jpeg_size == 0) {
      return false;
    }
    tjhandle tj_instance = tjInitDecompress();
    if (!tj_instance) {
      return false;
    }

    int subsamp = 0, colorspace = 0;
    if (
      tjDecompressHeader3(
        tj_instance, jpeg_data, static_cast<unsigned long>(jpeg_size),  // NOLINT(runtime/int)
        &width, &height,                                                // NOLINT(runtime/int)
        &subsamp,                                                       // NOLINT(runtime/int)
        &colorspace) !=                                                 // NOLINT(runtime/int)
      0) {
      tjDestroy(tj_instance);
      return false;
    }

    channels = 3;
    size_t required_size = static_cast<size_t>(width) * static_cast<size_t>(height) * channels;
    decoded_buffer_.resize(required_size);

    int status = tjDecompress2(
      tj_instance, jpeg_data, static_cast<unsigned long>(jpeg_size)  // NOLINT(runtime/int)
      ,
      decoded_buffer_.data(), width,  // NOLINT(runtime/int)
      0,                              // NOLINT(runtime/int)
      height,                         // NOLINT(runtime/int)
      TJPF_RGB,                       // NOLINT(runtime/int)
      TJFLAG_FASTDCT);
    tjDestroy(tj_instance);
    return (status == 0);
  }

  bool decode_and_verify_{true};
  size_t count_;
  std::vector<uint8_t> decoded_buffer_;

  protoros2::FlatSubscription<protoros2_example::msg::pb::CompressedImage>::SharedPtr flat_sub_;
  protoros2::ProtoSubscription<protoros2_example::msg::pb::CompressedImage>::SharedPtr proto_sub_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<EnterpriseImageListenerNode>());
  rclcpp::shutdown();
  return 0;
}
