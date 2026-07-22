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
#include <utility>

#include "rclcpp/rclcpp.hpp"

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
