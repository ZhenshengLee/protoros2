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
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include <turbojpeg.h>

#include <ament_index_cpp/get_package_share_path.hpp>
#include "protoros2/enterprise_node.hpp"
#include "protoros2_example/msg/compressed_image__typeadapter_protobuf_cpp.hpp"
#include "rclcpp/rclcpp.hpp"

using namespace std::chrono_literals;  // NOLINT(build/namespaces)

class EnterpriseImageTalkerNode : public protoros2::EnterpriseNode
{
public:
  EnterpriseImageTalkerNode() : EnterpriseNode("enterprise_image_talker_node"), count_(0)
  {
    this->declare_parameter<std::string>("topic_name", "enterprise_compressed_image");
    this->declare_parameter<std::string>("channel_type", "flat");
    this->declare_parameter<std::string>("image_path", "");
    this->declare_parameter<double>("frequency", 10.0);

    std::string topic_name = this->get_parameter("topic_name").as_string();
    std::string channel_type = this->get_parameter("channel_type").as_string();
    std::string image_param = this->get_parameter("image_path").as_string();
    double fps = this->get_parameter("frequency").as_double();

    resolved_image_path_ = resolve_image_path(image_param);
    if (!load_and_inspect_image(resolved_image_path_)) {
      RCLCPP_ERROR(
        this->get_logger(),
        "Failed to load image from path: '%s'. Please check your --ros-args -p image_path:=... parameter.",
        resolved_image_path_.c_str());
      throw std::runtime_error("Image loading failed");
    }

    if (channel_type == "proto") {
      proto_pub_ = this->create_proto_publisher<protoros2_example::msg::pb::CompressedImage>(topic_name, 10);
      use_flat_channel_ = false;
      RCLCPP_INFO(
        this->get_logger(), "Initialized ProtoChannel (Native Protobuf) publisher on topic '%s'", topic_name.c_str());
    } else {
      flat_pub_ = this->create_flat_publisher<protoros2_example::msg::pb::CompressedImage>(topic_name, 10);
      use_flat_channel_ = true;
      RCLCPP_INFO(
        this->get_logger(), "Initialized FlatChannel (Iceoryx Zero-Copy Bypass) publisher on topic '%s'",
        topic_name.c_str());
    }

    auto timer_callback = [this]() -> void {
      cached_msg_.Clear();
      auto now_ns = std::chrono::high_resolution_clock::now().time_since_epoch();
      auto sec_part = std::chrono::duration_cast<std::chrono::seconds>(now_ns);
      auto nsec_part = std::chrono::duration_cast<std::chrono::nanoseconds>(now_ns - sec_part);
      cached_msg_.mutable_timestamp()->set_seconds(sec_part.count());
      cached_msg_.mutable_timestamp()->set_nanos(nsec_part.count());
      cached_msg_.set_frame_id("camera_front");
      cached_msg_.set_format("jpeg");
      cached_msg_.set_data(image_bytes_.data(), image_bytes_.size());

      auto start_ts = std::chrono::high_resolution_clock::now();
      if (use_flat_channel_) {
        flat_pub_->publish(cached_msg_);
      } else {
        proto_pub_->publish(cached_msg_);
      }
      auto end_ts = std::chrono::high_resolution_clock::now();
      double publish_ms = std::chrono::duration<double, std::milli>(end_ts - start_ts).count();

      count_++;
      RCLCPP_INFO_THROTTLE(
        this->get_logger(), *this->get_clock(), 2000,
        "Published CompressedImage #%zu [%s] (%d x %d, %zu bytes) via %s in %.3f ms", count_,
        resolved_image_path_.c_str(), image_width_, image_height_, image_bytes_.size(),
        use_flat_channel_ ? "FlatChannel (Zero-Copy SHM)" : "ProtoChannel (Fast-Path)", publish_ms);
    };

    auto interval = std::chrono::duration<double>(1.0 / fps);
    timer_ = this->create_wall_timer(std::chrono::duration_cast<std::chrono::milliseconds>(interval), timer_callback);
  }

private:
  std::string resolve_image_path(const std::string & param_path)
  {
    if (param_path.empty()) {
      try {
        std::string share_dir = ament_index_cpp::get_package_share_path("protoros2_example").string();
        return share_dir + "/res/img/1920_1080.jpg";
      } catch (const std::exception &) {
        return "./res/img/1920_1080.jpg";
      }
    }

    std::ifstream direct_check(param_path, std::ios::binary);
    if (direct_check.is_open()) {
      return param_path;
    }
    try {
      std::string share_dir = ament_index_cpp::get_package_share_path("protoros2_example").string();
      std::string candidate1 = share_dir + "/res/img/" + param_path;
      std::ifstream c1_check(candidate1, std::ios::binary);
      if (c1_check.is_open()) {
        return candidate1;
      }
      std::string candidate2 = share_dir + "/" + param_path;
      std::ifstream c2_check(candidate2, std::ios::binary);
      if (c2_check.is_open()) {
        return candidate2;
      }
    } catch (const std::exception & e) {
      RCLCPP_DEBUG(this->get_logger(), "Share directory lookup warning: %s", e.what());
    }
    return param_path;
  }

  bool load_and_inspect_image(const std::string & path)
  {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
      return false;
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    if (size <= 0) {
      return false;
    }

    image_bytes_.resize(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char *>(image_bytes_.data()), size)) {
      return false;
    }

    tjhandle tj_instance = tjInitDecompress();
    if (tj_instance) {
      int subsamp = 0, colorspace = 0;
      if (
        tjDecompressHeader3(
          tj_instance, image_bytes_.data(), static_cast<unsigned long>(image_bytes_.size()),  // NOLINT(runtime/int)
          &image_width_,                                                                      // NOLINT(runtime/int)
          &image_height_, &subsamp, &colorspace) == 0) {                                      // NOLINT(runtime/int)
        image_channels_ = 3;
        RCLCPP_INFO(
          this->get_logger(),
          "Successfully loaded JPEG header from '%s' via TurboJPEG: %d x %d (%d components), total bytes: %zu",
          path.c_str(), image_width_, image_height_, image_channels_, image_bytes_.size());
      } else {
        image_width_ = 0;
        image_height_ = 0;
        image_channels_ = 0;
        RCLCPP_WARN(
          this->get_logger(), "Loaded raw binary image file '%s' (%zu bytes), but tjDecompressHeader3 failed: %s",
          path.c_str(), image_bytes_.size(), tjGetErrorStr2(tj_instance));
      }
      tjDestroy(tj_instance);
    }
    return true;
  }

  std::string resolved_image_path_;
  std::vector<uint8_t> image_bytes_;
  int image_width_{0};
  int image_height_{0};
  int image_channels_{0};
  bool use_flat_channel_{true};
  size_t count_;

  protoros2_example::msg::pb::CompressedImage cached_msg_;
  protoros2::FlatPublisher<protoros2_example::msg::pb::CompressedImage>::SharedPtr flat_pub_;
  protoros2::ProtoPublisher<protoros2_example::msg::pb::CompressedImage>::SharedPtr proto_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<EnterpriseImageTalkerNode>());
  rclcpp::shutdown();
  return 0;
}
