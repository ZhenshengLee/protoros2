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

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>

#include "protoros2/enterprise_node.hpp"
#include "protoros2_example/msg/bytes_payload__typeadapter_protobuf_cpp.hpp"
#include "rclcpp/rclcpp.hpp"

// Parameterizable bytes benchmark subscriber, counterpart of enterprise_bytes_talker.
// Reports transport+schedule latency, per-callback cost, throughput and frame drops.
// -p zero_copy_parse:=true switches the flat channel to aliasing (zero-copy) parsing.
class EnterpriseBytesListenerNode : public protoros2::EnterpriseNode
{
public:
  EnterpriseBytesListenerNode() : EnterpriseNode("enterprise_bytes_listener_node")
  {
    this->declare_parameter<std::string>("topic_name", "enterprise_bytes_payload");
    this->declare_parameter<std::string>("channel_type", "flat");
    this->declare_parameter<bool>("zero_copy_parse", true);
    this->declare_parameter<bool>("verify", true);

    const std::string topic_name = this->get_parameter("topic_name").as_string();
    const std::string channel_type = this->get_parameter("channel_type").as_string();
    const bool zero_copy_parse = this->get_parameter("zero_copy_parse").as_bool();
    verify_ = this->get_parameter("verify").as_bool();

    auto callback = [this](const protoros2_example::msg::pb::BytesPayload & msg) {
      auto callback_enter_ts = std::chrono::high_resolution_clock::now();

      // Transport + scheduling latency from the publisher's send timestamp.
      auto send_ns =
        std::chrono::seconds(msg.timestamp().seconds()) + std::chrono::nanoseconds(msg.timestamp().nanos());
      double transport_and_sched_ms =
        std::chrono::duration<double, std::milli>(callback_enter_ts.time_since_epoch() - send_ns).count();

      size_t payload_bytes = msg.data().size();
      bool pattern_ok = true;
      if (verify_ && !msg.data().empty()) {
        const auto & data = msg.data();
        const size_t last = data.size() - 1;
        const uint8_t expect_last = static_cast<uint8_t>((last * 31u + 7u) & 0xFFu);
        if (static_cast<uint8_t>(data[0]) != 7u || static_cast<uint8_t>(data[last]) != expect_last) {
          pattern_ok = false;
        }
        const uint32_t chunk_count = msg.chunk_count();
        if (pattern_ok && chunk_count > 0 && data.size() % chunk_count == 0) {
          const size_t chunk_size = data.size() / chunk_count;
          for (uint32_t i = 0; i < chunk_count; ++i) {
            if (static_cast<uint8_t>(data[i * chunk_size]) != 7u) {
              pattern_ok = false;
              break;
            }
          }
        }
        if (msg.chunk_hashes_size() != static_cast<int>(msg.chunk_count())) {
          pattern_ok = false;
        }
      }
      if (verify_ && !pattern_ok) {
        verify_failures_++;
      }

      if (last_seq_valid_ && msg.seq() > last_seq_ + 1) {
        dropped_ += msg.seq() - last_seq_ - 1;
      }
      last_seq_ = msg.seq();
      last_seq_valid_ = true;
      received_++;
      received_bytes_ += payload_bytes;

      auto callback_end_ts = std::chrono::high_resolution_clock::now();
      double callback_ms = std::chrono::duration<double, std::milli>(callback_end_ts - callback_enter_ts).count();

      latency_sum_ += transport_and_sched_ms;
      latency_min_ = std::min(latency_min_, transport_and_sched_ms);
      latency_max_ = std::max(latency_max_, transport_and_sched_ms);
      callback_sum_ += callback_ms;
      callback_min_ = std::min(callback_min_, callback_ms);
      callback_max_ = std::max(callback_max_, callback_ms);

      RCLCPP_INFO_THROTTLE(
        this->get_logger(), *this->get_clock(), 2000,
        "Received BytesPayload #%llu [%zu chunks, %zu bytes] | Transport & Sched Latency: %.3f ms | Callback: %.3f "
        "ms | Throughput: %.1f MB/s | Drops: %llu%s",
        static_cast<unsigned long long>(msg.seq()), static_cast<size_t>(msg.chunk_count()), payload_bytes,
        transport_and_sched_ms, callback_ms, throughput_mbps(), static_cast<unsigned long long>(dropped_),
        zero_copy_parse_active_ ? " | parse: zero-copy(aliasing)" : " | parse: copy");
    };

    if (channel_type == "proto") {
      if (zero_copy_parse) {
        RCLCPP_WARN(
          this->get_logger(), "zero_copy_parse is currently supported on the flat channel only; ignored for proto");
      }
      proto_sub_ = this->create_proto_subscription<protoros2_example::msg::pb::BytesPayload>(topic_name, 10, callback);
      RCLCPP_INFO(this->get_logger(), "Initialized ProtoChannel subscription on topic '%s'", topic_name.c_str());
    } else {
      flat_sub_ = this->create_flat_subscription<protoros2_example::msg::pb::BytesPayload>(topic_name, 10, callback);
      flat_sub_->set_zero_copy_parse(zero_copy_parse);
      zero_copy_parse_active_ = zero_copy_parse;
      RCLCPP_INFO(
        this->get_logger(), "Initialized FlatChannel subscription on topic '%s' (zero_copy_parse=%s)",
        topic_name.c_str(), zero_copy_parse ? "true" : "false");
    }
  }

  ~EnterpriseBytesListenerNode() override
  {
    if (received_ > 0) {
      double n = static_cast<double>(received_);
      RCLCPP_INFO(
        rclcpp::get_logger("enterprise_bytes_listener"),
        "BENCH SUMMARY: count=%llu drops=%llu verify_failures=%llu | latency avg/min/max: %.3f/%.3f/%.3f ms | "
        "callback avg/min/max: %.3f/%.3f/%.3f ms | parse: %s",
        static_cast<unsigned long long>(received_), static_cast<unsigned long long>(dropped_),
        static_cast<unsigned long long>(verify_failures_), latency_sum_ / n, latency_min_, latency_max_,
        callback_sum_ / n, callback_min_, callback_max_, zero_copy_parse_active_ ? "zero-copy(aliasing)" : "copy");
    }
  }

private:
  double throughput_mbps()
  {
    auto now = std::chrono::steady_clock::now();
    if (window_start_ == std::chrono::steady_clock::time_point{}) {
      window_start_ = now;
      window_bytes_ = 0;
    }
    window_bytes_ += last_payload_window_bytes();
    double elapsed_s = std::chrono::duration<double>(now - window_start_).count();
    if (elapsed_s >= 2.0) {
      window_mbps_ = static_cast<double>(window_bytes_) / elapsed_s / 1e6;
      window_start_ = now;
      window_bytes_ = 0;
    }
    return window_mbps_;
  }

  size_t last_payload_window_bytes()
  {
    size_t delta = received_bytes_ - accounted_bytes_;
    accounted_bytes_ = received_bytes_;
    return delta;
  }

  bool verify_{true};
  bool zero_copy_parse_active_{false};
  bool last_seq_valid_{false};
  uint64_t last_seq_{0};
  uint64_t received_{0};
  uint64_t dropped_{0};
  uint64_t verify_failures_{0};
  size_t received_bytes_{0};
  size_t accounted_bytes_{0};
  size_t window_bytes_{0};
  double window_mbps_{0.0};
  std::chrono::steady_clock::time_point window_start_{};
  double latency_sum_{0.0};
  double latency_min_{std::numeric_limits<double>::max()};
  double latency_max_{0.0};
  double callback_sum_{0.0};
  double callback_min_{std::numeric_limits<double>::max()};
  double callback_max_{0.0};

  protoros2::FlatSubscription<protoros2_example::msg::pb::BytesPayload>::SharedPtr flat_sub_;
  protoros2::ProtoSubscription<protoros2_example::msg::pb::BytesPayload>::SharedPtr proto_sub_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<EnterpriseBytesListenerNode>());
  rclcpp::shutdown();
  return 0;
}
