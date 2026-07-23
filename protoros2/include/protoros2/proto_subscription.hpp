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

#ifndef PROTOROS2__PROTO_SUBSCRIPTION_HPP_
#define PROTOROS2__PROTO_SUBSCRIPTION_HPP_

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp/serialized_message.hpp"
#include "rclcpp/type_adapter.hpp"

namespace protoros2
{

/**
 * @brief ProtoSubscription wraps an underlying rclcpp::SubscriptionBase created
 * for either rclcpp::SerializedMessage (Fast-Path) or RosMsgT/ProtoMsgT (Graceful Fallback).
 *
 * @tparam ProtoMsgT The C++ Protobuf message class.
 * @tparam RosMsgT The ROS 2 message struct class (or ProtoMsgT if using TypeAdapter directly).
 */
template <typename ProtoMsgT, typename RosMsgT = ProtoMsgT>
class ProtoSubscription
{
public:
  using SharedPtr = std::shared_ptr<ProtoSubscription<ProtoMsgT, RosMsgT>>;

  ProtoSubscription(rclcpp::SubscriptionBase::SharedPtr sub_base, bool is_protobuf_native)
  : sub_base_(std::move(sub_base)), is_protobuf_native_(is_protobuf_native)
  {
  }

  /**
   * @brief Take a message directly from the subscription queue without callback dispatch.
   * Useful when using rclcpp::WaitSet or Polling Subscriber patterns.
   *
   * @param out_msg Reference to the Protobuf message to populate.
   * @param message_info Reference to the MessageInfo structure to populate.
   * @return true if a message was successfully taken and parsed, false otherwise.
   */
  bool take(ProtoMsgT & out_msg, rclcpp::MessageInfo & message_info)
  {
    if (!sub_base_) {
      return false;
    }
    if (is_protobuf_native_) {
      rclcpp::SerializedMessage ser_msg;
      if (sub_base_->take_serialized(ser_msg, message_info)) {
        const auto & rcl_msg = ser_msg.get_rcl_serialized_message();
        out_msg.Clear();
        return out_msg.ParseFromArray(rcl_msg.buffer, static_cast<int>(rcl_msg.buffer_length));
      }
    } else {
      if constexpr (
        std::is_same_v<ProtoMsgT, RosMsgT> || rclcpp::TypeAdapter<ProtoMsgT, RosMsgT>::is_specialized::value) {
        auto * proto_sub = dynamic_cast<rclcpp::Subscription<ProtoMsgT> *>(sub_base_.get());
        if (proto_sub) {
          ProtoMsgT taken_msg;
          if (proto_sub->take(taken_msg, message_info)) {
            out_msg = std::move(taken_msg);
            return true;
          }
        }
      } else {
        auto * ros_sub = dynamic_cast<rclcpp::Subscription<RosMsgT> *>(sub_base_.get());
        if (ros_sub) {
          RosMsgT taken_ros;
          if (ros_sub->take(taken_ros, message_info)) {
            out_msg.Clear();
            rclcpp::TypeAdapter<ProtoMsgT, RosMsgT>::convert_to_custom_type(taken_ros, out_msg);
            return true;
          }
        }
      }
    }
    return false;
  }

  /**
   * @brief Overload of take() without requiring MessageInfo output.
   */
  bool take(ProtoMsgT & out_msg)
  {
    rclcpp::MessageInfo dummy_info;
    return take(out_msg, dummy_info);
  }

  /// Get the underlying SubscriptionBase pointer.
  rclcpp::SubscriptionBase::SharedPtr get_subscription_base() const { return sub_base_; }

  /// Check whether the subscription is operating in native Protobuf mode.
  bool is_protobuf_native() const { return is_protobuf_native_; }

  /// Get topic name of the subscription.
  const char * get_topic_name() const { return sub_base_ ? sub_base_->get_topic_name() : ""; }

private:
  rclcpp::SubscriptionBase::SharedPtr sub_base_;
  bool is_protobuf_native_{false};
};

}  // namespace protoros2

#endif  // PROTOROS2__PROTO_SUBSCRIPTION_HPP_
