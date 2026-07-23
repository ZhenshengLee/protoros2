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

#ifndef PROTOROS2__PROTO_POLLING_SUBSCRIBER_HPP_
#define PROTOROS2__PROTO_POLLING_SUBSCRIBER_HPP_

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <stdexcept>

#include "rclcpp/rclcpp.hpp"
#include "protoros2/enterprise_node.hpp"

namespace protoros2
{

namespace polling_policy
{
// Forward declaration of the policy templates so we can use them as default arguments
template <typename ProtoMsgT, typename RosMsgT>
class Latest;
template <typename ProtoMsgT, typename RosMsgT>
class Newest;
template <typename ProtoMsgT, typename RosMsgT>
class All;
}  // namespace polling_policy

template <
  typename ProtoMsgT, typename RosMsgT = ProtoMsgT,
  template <typename, typename> class PollingPolicy = polling_policy::Latest>
class ProtoPollingSubscriber;

namespace polling_policy
{
template <typename ProtoMsgT, typename RosMsgT>
class Latest
{
private:
  std::shared_ptr<ProtoMsgT> data_{nullptr};
  std::optional<rclcpp::Time> timestamp_{std::nullopt};

public:
  std::shared_ptr<const ProtoMsgT> take_data()
  {
    auto * sub = static_cast<ProtoPollingSubscriber<ProtoMsgT, RosMsgT, polling_policy::Latest> *>(this);
    ProtoMsgT new_data;
    rclcpp::MessageInfo message_info;
    if (sub->subscriber()->take(new_data, message_info)) {
      data_ = std::make_shared<ProtoMsgT>(std::move(new_data));
      timestamp_ = rclcpp::Time(message_info.get_rmw_message_info().source_timestamp, RCL_ROS_TIME);
    }
    return data_;
  }
  std::optional<rclcpp::Time> last_taken_data_timestamp() const { return timestamp_; }
};

template <typename ProtoMsgT, typename RosMsgT>
class Newest
{
private:
  std::optional<rclcpp::Time> timestamp_{std::nullopt};

public:
  std::shared_ptr<const ProtoMsgT> take_data()
  {
    auto * sub = static_cast<ProtoPollingSubscriber<ProtoMsgT, RosMsgT, polling_policy::Newest> *>(this);
    ProtoMsgT new_data;
    rclcpp::MessageInfo message_info;
    if (sub->subscriber()->take(new_data, message_info)) {
      timestamp_ = rclcpp::Time(message_info.get_rmw_message_info().source_timestamp, RCL_ROS_TIME);
      return std::make_shared<ProtoMsgT>(std::move(new_data));
    }
    timestamp_ = std::nullopt;
    return nullptr;
  }
  std::optional<rclcpp::Time> last_taken_data_timestamp() const { return timestamp_; }
};

template <typename ProtoMsgT, typename RosMsgT>
class All
{
private:
  std::optional<rclcpp::Time> timestamp_{std::nullopt};

public:
  std::vector<std::shared_ptr<const ProtoMsgT>> take_data()
  {
    auto * sub = static_cast<ProtoPollingSubscriber<ProtoMsgT, RosMsgT, polling_policy::All> *>(this);
    std::vector<std::shared_ptr<const ProtoMsgT>> data;
    rclcpp::MessageInfo message_info;
    for (;;) {
      ProtoMsgT datum;
      if (sub->subscriber()->take(datum, message_info)) {
        data.push_back(std::make_shared<ProtoMsgT>(std::move(datum)));
        timestamp_ = rclcpp::Time(message_info.get_rmw_message_info().source_timestamp, RCL_ROS_TIME);
      } else {
        break;
      }
    }
    if (data.empty()) {
      timestamp_ = std::nullopt;
    }
    return data;
  }
  std::optional<rclcpp::Time> last_taken_data_timestamp() const { return timestamp_; }
};
}  // namespace polling_policy

/**
 * @brief ProtoPollingSubscriber adapts the high-speed ProtoSubscription (Fast-Path SerializedMessage bypass)
 * into a Polling mode compatible with autoware_utils::InterProcessPollingSubscriber patterns.
 *
 * @tparam ProtoMsgT The Protobuf message type.
 * @tparam RosMsgT The ROS 2 message struct class (defaults to ProtoMsgT).
 */
template <typename ProtoMsgT, typename RosMsgT, template <typename, typename> class PollingPolicy>
class ProtoPollingSubscriber : public PollingPolicy<ProtoMsgT, RosMsgT>
{
private:
  typename ProtoSubscription<ProtoMsgT, RosMsgT>::SharedPtr subscriber_;

public:
  using SharedPtr = std::shared_ptr<ProtoPollingSubscriber<ProtoMsgT, RosMsgT, PollingPolicy>>;

  explicit ProtoPollingSubscriber(
    EnterpriseNode * node, const std::string & topic_name, const rclcpp::QoS & qos = rclcpp::QoS{1})
  {
    auto noexec_callback_group = node->create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive, false);

    auto noexec_subscription_options = rclcpp::SubscriptionOptions();
    noexec_subscription_options.callback_group = noexec_callback_group;

    auto empty_callback = [](const ProtoMsgT &) { assert(false); };

    subscriber_ = node->template create_proto_subscription<ProtoMsgT, RosMsgT>(
      topic_name, qos, empty_callback, noexec_subscription_options);
  }

  static SharedPtr create_subscription(
    EnterpriseNode * node, const std::string & topic_name, const rclcpp::QoS & qos = rclcpp::QoS{1})
  {
    return std::make_shared<ProtoPollingSubscriber<ProtoMsgT, RosMsgT, PollingPolicy>>(node, topic_name, qos);
  }

  typename ProtoSubscription<ProtoMsgT, RosMsgT>::SharedPtr subscriber() { return subscriber_; }
};

}  // namespace protoros2

#endif  // PROTOROS2__PROTO_POLLING_SUBSCRIBER_HPP_
