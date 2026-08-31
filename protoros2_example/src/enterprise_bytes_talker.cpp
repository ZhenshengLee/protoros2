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
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "protoros2/enterprise_node.hpp"
#include "protoros2_example/msg/bytes_payload__typeadapter_protobuf_cpp.hpp"
#include "rclcpp/rclcpp.hpp"

// Parameterizable bytes benchmark publisher (no image dependency). Combined with
// enterprise_bytes_listener it covers three benchmark tiers:
//   shallow large:   -p payload_size:=3145728 -p chunk_count:=1  -p frequency:=10.0
//   nested medium:   -p payload_size:=1048576 -p chunk_count:=64 -p frequency:=30.0
//   high-rate small: -p payload_size:=2048    -p chunk_count:=1  -p frequency:=200.0
class EnterpriseBytesTalkerNode : public protoros2::EnterpriseNode
{
public:
  EnterpriseBytesTalkerNode() : EnterpriseNode("enterprise_bytes_talker_node")
  {
    this->declare_parameter<std::string>("topic_name", "enterprise_bytes_payload");
    this->declare_parameter<std::string>("channel_type", "flat");
    this->declare_parameter<int64_t>("payload_size", 3 * 1024 * 1024);
    this->declare_parameter<int64_t>("chunk_count", 1);
    this->declare_parameter<double>("frequency", 10.0);

    const std::string topic_name = this->get_parameter("topic_name").as_string();
    const std::string channel_type = this->get_parameter("channel_type").as_string();
    const int64_t payload_size = this->get_parameter("payload_size").as_int();
    const int64_t chunk_count = this->get_parameter("chunk_count").as_int();
    const double fps = this->get_parameter("frequency").as_double();

    if (payload_size <= 0 || chunk_count <= 0 || fps <= 0.0) {
      RCLCPP_ERROR(this->get_logger(), "payload_size / chunk_count / frequency must all be positive");
      throw std::runtime_error("Invalid bytes benchmark parameters");
    }

    chunk_size_ = static_cast<size_t>(payload_size) / static_cast<size_t>(chunk_count);
    if (chunk_size_ == 0) {
      chunk_size_ = 1;
    }
    chunk_count_ = static_cast<size_t>(chunk_count);
    total_bytes_ = chunk_size_ * chunk_count_;

    // Deterministic fill pattern; the listener spot-checks it. Prebuild the whole blob
    // once (filler repeated per chunk) plus one FNV-1a hash per chunk.
    std::vector<uint8_t> filler(chunk_size_);
    for (size_t i = 0; i < chunk_size_; ++i) {
      filler[i] = static_cast<uint8_t>((i * 31u + 7u) & 0xFFu);
    }
    blob_.reserve(total_bytes_);
    for (size_t i = 0; i < chunk_count_; ++i) {
      blob_.insert(blob_.end(), filler.begin(), filler.end());
    }
    uint64_t fnv = 14695981039346656037ULL;
    for (uint8_t b : filler) {
      fnv ^= b;
      fnv *= 1099511628211ULL;
    }
    chunk_hashes_.assign(chunk_count_, fnv);

    if (channel_type == "proto") {
      proto_pub_ = this->create_proto_publisher<protoros2_example::msg::pb::BytesPayload>(topic_name, 10);
      use_flat_channel_ = false;
      RCLCPP_INFO(this->get_logger(), "Initialized ProtoChannel publisher on topic '%s'", topic_name.c_str());
    } else {
      flat_pub_ = this->create_flat_publisher<protoros2_example::msg::pb::BytesPayload>(topic_name, 10);
      use_flat_channel_ = true;
      RCLCPP_INFO(this->get_logger(), "Initialized FlatChannel publisher on topic '%s'", topic_name.c_str());
    }

    RCLCPP_INFO(
      this->get_logger(), "Benchmark profile: %zu chunks x %zu bytes = %zu bytes @ %.1f Hz (%.1f MB/s nominal)",
      chunk_count_, chunk_size_, total_bytes_, fps, static_cast<double>(total_bytes_) * fps / 1e6);

    auto timer_callback = [this]() -> void {
      cached_msg_.Clear();
      auto now_ns = std::chrono::high_resolution_clock::now().time_since_epoch();
      auto sec_part = std::chrono::duration_cast<std::chrono::seconds>(now_ns);
      auto nsec_part = std::chrono::duration_cast<std::chrono::nanoseconds>(now_ns - sec_part);
      cached_msg_.mutable_timestamp()->set_seconds(sec_part.count());
      cached_msg_.mutable_timestamp()->set_nanos(nsec_part.count());
      cached_msg_.set_seq(seq_);
      cached_msg_.set_data(blob_.data(), blob_.size());
      cached_msg_.set_chunk_count(static_cast<uint32_t>(chunk_count_));
      for (uint64_t hash : chunk_hashes_) {
        cached_msg_.add_chunk_hashes(hash);
      }

      auto start_ts = std::chrono::high_resolution_clock::now();
      if (use_flat_channel_) {
        flat_pub_->publish(cached_msg_);
      } else {
        proto_pub_->publish(cached_msg_);
      }
      auto end_ts = std::chrono::high_resolution_clock::now();
      double publish_ms = std::chrono::duration<double, std::milli>(end_ts - start_ts).count();

      seq_++;
      RCLCPP_INFO_THROTTLE(
        this->get_logger(), *this->get_clock(), 2000,
        "Published BytesPayload #%llu (%zu chunks x %zu bytes) via %s in %.3f ms",
        static_cast<unsigned long long>(seq_), chunk_count_, chunk_size_,
        use_flat_channel_ ? "FlatChannel (Zero-Copy SHM)" : "ProtoChannel (Fast-Path)", publish_ms);
    };

    auto interval = std::chrono::duration<double>(1.0 / fps);
    timer_ = this->create_wall_timer(std::chrono::duration_cast<std::chrono::milliseconds>(interval), timer_callback);
  }

private:
  size_t chunk_size_{0};
  size_t chunk_count_{0};
  size_t total_bytes_{0};
  uint64_t seq_{0};
  bool use_flat_channel_{true};
  std::vector<uint8_t> blob_;
  std::vector<uint64_t> chunk_hashes_;

  protoros2_example::msg::pb::BytesPayload cached_msg_;
  protoros2::FlatPublisher<protoros2_example::msg::pb::BytesPayload>::SharedPtr flat_pub_;
  protoros2::ProtoPublisher<protoros2_example::msg::pb::BytesPayload>::SharedPtr proto_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<EnterpriseBytesTalkerNode>());
  rclcpp::shutdown();
  return 0;
}
