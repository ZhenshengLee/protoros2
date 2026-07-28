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

#ifndef PROTOROS2__PROTO_PUBLISHER_HPP_
#define PROTOROS2__PROTO_PUBLISHER_HPP_

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp/serialized_message.hpp"
#include "rclcpp/type_adapter.hpp"
#include "rmw/rmw.h"

namespace protoros2
{

/**
 * @brief ProtoPublisher wraps an underlying rclcpp::Publisher and implements
 * dual-track smart routing based on the runtime RMW serialization format.
 *
 * When the underlying RMW speaks Protobuf natively (e.g., rmw_ecal_proto_cpp, rmw_iceoryx_proto_cpp),
 * ProtoPublisher short-circuits the standard TypeAdapter / CDR conversion path and directly
 * publishes the serialized Protobuf bytes inside an rclcpp::SerializedMessage (Fast-Path).
 * When running on standard RMW implementations (e.g., FastRTPS, CycloneDDS), it gracefully falls back
 * to standard TypeAdapter / ROS message publishing (Graceful Fallback).
 *
 * @tparam ProtoMsgT The C++ Protobuf message class.
 * @tparam RosMsgT The ROS 2 message struct class (or ProtoMsgT if using TypeAdapter directly).
 */
template <typename ProtoMsgT, typename RosMsgT = ProtoMsgT>
class ProtoPublisher
{
public:
  using SharedPtr = std::shared_ptr<ProtoPublisher<ProtoMsgT, RosMsgT>>;
  using RawPublisherPtr = typename rclcpp::Publisher<ProtoMsgT>::SharedPtr;

  explicit ProtoPublisher(RawPublisherPtr pub) : pub_(std::move(pub))
  {
    const char * rmw_format = rmw_get_serialization_format();
    is_protobuf_native_ = (rmw_format != nullptr && std::string(rmw_format) == "protobuf");
  }

  /**
   * @brief Publish a Protobuf message instance using smart routing.
   *
   * @param proto_msg The Protobuf message to publish.
   */
  void publish(const ProtoMsgT & proto_msg)
  {
    if (!pub_) {
      throw std::runtime_error("ProtoPublisher underlying publisher is null");
    }

    if (is_protobuf_native_) {
      // ==========================================
      // Track A: Protobuf Fast-Path (0 TypeAdapter conversions)
      // ==========================================
      size_t size = proto_msg.ByteSizeLong();
      rclcpp::SerializedMessage serialized_msg(size);

      // Serialize directly to the rcl_serialized_message_t buffer
      proto_msg.SerializeToArray(serialized_msg.get_rcl_serialized_message().buffer, static_cast<int>(size));
      serialized_msg.get_rcl_serialized_message().buffer_length = size;

      // Publish as SerializedMessage, bypassing rcl TypeAdapter conversion
      pub_->publish(serialized_msg);
    } else {
      // ==========================================
      // Track B: Graceful Fallback (CDR / TypeAdapter mode)
      // ==========================================
      pub_->publish(proto_msg);
    }
  }

  /**
   * @brief Publish a Protobuf message instance (unique_ptr) using smart routing for zero-copy intra-process.
   *
   * @param proto_msg The Protobuf message to publish.
   */
  void publish(std::unique_ptr<ProtoMsgT> proto_msg)
  {
    if (!pub_) {
      throw std::runtime_error("ProtoPublisher underlying publisher is null");
    }

    if (is_protobuf_native_) {
      // If there are intra-process subscriptions and TypeAdapter is available,
      // we must pass the unique_ptr to rclcpp to allow zero-copy delivery,
      // bypassing the manual serialization.
      bool use_intra_process = false;
      if constexpr (
        std::is_same_v<ProtoMsgT, RosMsgT> || rclcpp::TypeAdapter<ProtoMsgT, RosMsgT>::is_specialized::value) {
        if (pub_->get_intra_process_subscription_count() > 0) {
          use_intra_process = true;
        }
      }

      if (use_intra_process) {
        pub_->publish(std::move(proto_msg));
        return;
      }

      // ==========================================
      // Track A: Protobuf Fast-Path (0 TypeAdapter conversions)
      // ==========================================
      size_t size = proto_msg->ByteSizeLong();
      rclcpp::SerializedMessage serialized_msg(size);

      // Serialize directly to the rcl_serialized_message_t buffer
      proto_msg->SerializeToArray(serialized_msg.get_rcl_serialized_message().buffer, static_cast<int>(size));
      serialized_msg.get_rcl_serialized_message().buffer_length = size;

      // Publish as SerializedMessage, bypassing rcl TypeAdapter conversion
      pub_->publish(serialized_msg);
    } else {
      // ==========================================
      // Track B: Graceful Fallback (CDR / TypeAdapter mode)
      // ==========================================
      pub_->publish(std::move(proto_msg));
    }
  }

  /// Get the raw underlying rclcpp::Publisher instance.
  RawPublisherPtr get_raw_publisher() const { return pub_; }

  /// Get the underlying publisher as PublisherBase.
  rclcpp::PublisherBase::SharedPtr get_publisher_base() const { return pub_; }

  /// Check whether the current RMW is running in native Protobuf mode.
  bool is_protobuf_native() const { return is_protobuf_native_; }

  /// Get subscription count on this topic.
  size_t get_subscription_count() const { return pub_ ? pub_->get_subscription_count() : 0; }

  /// Get intra-process subscription count.
  size_t get_intra_process_subscription_count() const
  {
    return pub_ ? pub_->get_intra_process_subscription_count() : 0;
  }

private:
  RawPublisherPtr pub_;
  bool is_protobuf_native_{false};
};

}  // namespace protoros2

#endif  // PROTOROS2__PROTO_PUBLISHER_HPP_
