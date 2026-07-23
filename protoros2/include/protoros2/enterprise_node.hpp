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

#ifndef PROTOROS2__ENTERPRISE_NODE_HPP_
#define PROTOROS2__ENTERPRISE_NODE_HPP_

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp/serialized_message.hpp"
#include "rclcpp/type_adapter.hpp"
#include "rmw/rmw.h"

#include "protoros2/proto_publisher.hpp"
#include "protoros2/proto_subscription.hpp"
#include "protoros2/flat_publisher.hpp"
#include "protoros2/flat_subscription.hpp"

namespace protoros2
{

/**
 * @brief EnterpriseNode extends rclcpp::Node to provide smart multi-channel communication
 * for Protobuf messages in high-performance robotics and autonomous driving systems.
 *
 * EnterpriseNode orchestrates multiple communication channels:
 * 1. ProtoChannel (create_proto_publisher / create_proto_subscription):
 *    Transparently inspects the runtime RMW serialization format:
 *    - If RMW format is "protobuf", registers publishers and subscribers using
 *      rclcpp::SerializedMessage directly, achieving 0-copy / 0-conversion Protobuf Fast-Path.
 *    - Otherwise, falls back to standard rclcpp::TypeAdapter / CDR publishing and subscribing.
 * 2. FlatChannel (create_flat_publisher / create_flat_subscription):
 *    High-performance bypass communication channel (e.g., based on Iceoryx / shared memory IPC)
 *    designed for ultra-low latency data transmission. Supports both high-speed callback push
 *    and WaitSet / Polling / CallbackGroup / EventsExecutor paradigms.
 */
class EnterpriseNode : public rclcpp::Node
{
public:
  using rclcpp::Node::Node;

  /**
   * @brief Create a ProtoPublisher with smart routing capabilities.
   *
   * @tparam ProtoMsgT The C++ Protobuf message class.
   * @tparam RosMsgT The ROS 2 message struct class (defaults to ProtoMsgT).
   * @param topic_name The topic name to publish on.
   * @param qos Quality of Service settings.
   * @param options Publisher options.
   * @return Shared pointer to the created ProtoPublisher.
   */
  template <typename ProtoMsgT, typename RosMsgT = ProtoMsgT>
  std::shared_ptr<ProtoPublisher<ProtoMsgT, RosMsgT>> create_proto_publisher(
    const std::string & topic_name, const rclcpp::QoS & qos,
    const rclcpp::PublisherOptions & options = rclcpp::PublisherOptions())
  {
    auto raw_pub = this->create_publisher<RosMsgT>(topic_name, qos, options);
    return std::make_shared<ProtoPublisher<ProtoMsgT, RosMsgT>>(raw_pub);
  }

  /**
   * @brief Create a ProtoSubscription with smart dual-track callback routing.
   *
   * @tparam ProtoMsgT The C++ Protobuf message class.
   * @tparam RosMsgT The ROS 2 message struct class (defaults to ProtoMsgT).
   * @tparam CallbackT The user callback type.
   * @param topic_name The topic name to subscribe to.
   * @param qos Quality of Service settings.
   * @param callback The callback invoked when a message is received.
   * @param options Subscription options.
   * @return Shared pointer to the created ProtoSubscription.
   */
  template <typename ProtoMsgT, typename RosMsgT = ProtoMsgT, typename CallbackT>
  std::shared_ptr<ProtoSubscription<ProtoMsgT, RosMsgT>> create_proto_subscription(
    const std::string & topic_name, const rclcpp::QoS & qos, CallbackT && callback,
    const rclcpp::SubscriptionOptions & options = rclcpp::SubscriptionOptions())
  {
    const char * rmw_format = rmw_get_serialization_format();
    bool is_protobuf_native = (rmw_format != nullptr && std::string(rmw_format) == "protobuf");

    if (is_protobuf_native) {
      // Track A: Protobuf Fast-Path (subscribe to SerializedMessage and parse directly)
      auto serialized_callback = [user_callback = std::forward<CallbackT>(callback)](
                                   std::shared_ptr<rclcpp::SerializedMessage> serialized_msg,
                                   const rclcpp::MessageInfo & message_info) -> void {
        if (!serialized_msg) {
          return;
        }
        ProtoMsgT proto_msg;
        const auto & rcl_msg = serialized_msg->get_rcl_serialized_message();
        if (proto_msg.ParseFromArray(rcl_msg.buffer, static_cast<int>(rcl_msg.buffer_length))) {
          if constexpr (std::is_invocable_v<CallbackT, const ProtoMsgT &, const rclcpp::MessageInfo &>) {
            user_callback(proto_msg, message_info);
          } else if constexpr (std::is_invocable_v<CallbackT, const ProtoMsgT &>) {
            user_callback(proto_msg);
          } else if constexpr (std::is_invocable_v<
                                 CallbackT, std::shared_ptr<ProtoMsgT>, const rclcpp::MessageInfo &>) {
            user_callback(std::make_shared<ProtoMsgT>(std::move(proto_msg)), message_info);
          } else if constexpr (std::is_invocable_v<CallbackT, std::shared_ptr<ProtoMsgT>>) {
            user_callback(std::make_shared<ProtoMsgT>(std::move(proto_msg)));
          } else {
            static_assert(
              std::is_invocable_v<CallbackT, const ProtoMsgT &> ||
                std::is_invocable_v<CallbackT, const ProtoMsgT &, const rclcpp::MessageInfo &> ||
                std::is_invocable_v<CallbackT, std::shared_ptr<ProtoMsgT>> ||
                std::is_invocable_v<CallbackT, std::shared_ptr<ProtoMsgT>, const rclcpp::MessageInfo &>,
              "Unsupported callback signature for ProtoSubscription");
          }
        } else {
          RCLCPP_ERROR(
            rclcpp::get_logger("ProtoSubscription"),
            "Failed to parse Protobuf message from SerializedMessage buffer (length: %zu)",
            static_cast<size_t>(rcl_msg.buffer_length));
        }
      };

      auto sub = this->create_subscription<RosMsgT>(topic_name, qos, serialized_callback, options);
      return std::make_shared<ProtoSubscription<ProtoMsgT, RosMsgT>>(sub, true);
    } else {
      // Track B: Graceful Fallback (subscribe to RosMsgT or ProtoMsgT via TypeAdapter)
      if constexpr (
        std::is_same_v<ProtoMsgT, RosMsgT> || rclcpp::TypeAdapter<ProtoMsgT, RosMsgT>::is_specialized::value) {
        auto sub = this->create_subscription<ProtoMsgT>(topic_name, qos, std::forward<CallbackT>(callback), options);
        return std::make_shared<ProtoSubscription<ProtoMsgT, RosMsgT>>(sub, false);
      } else {
        auto ros_callback = [user_callback = std::forward<CallbackT>(callback)](
                              const std::shared_ptr<RosMsgT> & ros_msg,
                              const rclcpp::MessageInfo & message_info) -> void {
          if (!ros_msg) {
            return;
          }
          ProtoMsgT proto_msg;
          rclcpp::TypeAdapter<ProtoMsgT, RosMsgT>::convert_to_custom_type(*ros_msg, proto_msg);
          if constexpr (std::is_invocable_v<CallbackT, const ProtoMsgT &, const rclcpp::MessageInfo &>) {
            user_callback(proto_msg, message_info);
          } else if constexpr (std::is_invocable_v<CallbackT, const ProtoMsgT &>) {
            user_callback(proto_msg);
          } else if constexpr (std::is_invocable_v<
                                 CallbackT, std::shared_ptr<ProtoMsgT>, const rclcpp::MessageInfo &>) {
            user_callback(std::make_shared<ProtoMsgT>(std::move(proto_msg)), message_info);
          } else if constexpr (std::is_invocable_v<CallbackT, std::shared_ptr<ProtoMsgT>>) {
            user_callback(std::make_shared<ProtoMsgT>(std::move(proto_msg)));
          }
        };

        auto sub = this->create_subscription<RosMsgT>(topic_name, qos, ros_callback, options);
        return std::make_shared<ProtoSubscription<ProtoMsgT, RosMsgT>>(sub, false);
      }
    }
  }

  /**
   * @brief Create a FlatPublisher for high-performance bypass communication (e.g., Iceoryx).
   *
   * @tparam ProtoMsgT The C++ Protobuf message class.
   * @tparam RosMsgT The ROS 2 message struct class (defaults to ProtoMsgT).
   * @param topic_name The topic name to publish on.
   * @param qos Quality of Service settings.
   * @param options Publisher options.
   * @return Shared pointer to the created FlatPublisher.
   */
  template <typename ProtoMsgT, typename RosMsgT = ProtoMsgT>
  std::shared_ptr<FlatPublisher<ProtoMsgT, RosMsgT>> create_flat_publisher(
    const std::string & topic_name, const rclcpp::QoS & qos,
    const rclcpp::PublisherOptions & options = rclcpp::PublisherOptions())
  {
    (void)qos;
    (void)options;
    return std::make_shared<FlatPublisher<ProtoMsgT, RosMsgT>>(topic_name);
  }

  /**
   * @brief Create a FlatSubscription with callback for high-performance bypass communication.
   * Supports CallbackGroup registration and both CallbackPush and WaitSetPull modes.
   *
   * @tparam ProtoMsgT The C++ Protobuf message class.
   * @tparam RosMsgT The ROS 2 message struct class (defaults to ProtoMsgT).
   * @tparam CallbackT The user callback type.
   * @param topic_name The topic name to subscribe to.
   * @param qos Quality of Service settings.
   * @param callback The callback invoked when a message is received.
   * @param options Subscription options (including callback_group).
   * @param mode Optional mode selection (CallbackPush vs WaitSetPull).
   * @return Shared pointer to the created FlatSubscription.
   */
  template <typename ProtoMsgT, typename RosMsgT = ProtoMsgT, typename CallbackT>
  std::shared_ptr<FlatSubscription<ProtoMsgT, RosMsgT>> create_flat_subscription(
    const std::string & topic_name, const rclcpp::QoS & qos, CallbackT && callback,
    const rclcpp::SubscriptionOptions & options = rclcpp::SubscriptionOptions(),
    FlatSubscriptionMode mode = FlatSubscriptionMode::CallbackPush)
  {
    (void)qos;
    if (options.callback_group != nullptr) {
      mode = FlatSubscriptionMode::WaitSetPull;
    }

    auto cb_wrapper = [user_callback = std::forward<CallbackT>(callback)](const ProtoMsgT & proto_msg) -> void {
      if constexpr (std::is_invocable_v<CallbackT, const ProtoMsgT &, const rclcpp::MessageInfo &>) {
        rclcpp::MessageInfo dummy_info;
        user_callback(proto_msg, dummy_info);
      } else if constexpr (std::is_invocable_v<CallbackT, const ProtoMsgT &>) {
        user_callback(proto_msg);
      } else if constexpr (std::is_invocable_v<CallbackT, std::shared_ptr<ProtoMsgT>, const rclcpp::MessageInfo &>) {
        rclcpp::MessageInfo dummy_info;
        user_callback(std::make_shared<ProtoMsgT>(proto_msg), dummy_info);
      } else if constexpr (std::is_invocable_v<CallbackT, std::shared_ptr<ProtoMsgT>>) {
        user_callback(std::make_shared<ProtoMsgT>(proto_msg));
      }
    };

    auto sub = std::make_shared<FlatSubscription<ProtoMsgT, RosMsgT>>(topic_name, cb_wrapper, mode);
    if (options.callback_group != nullptr || mode == FlatSubscriptionMode::WaitSetPull) {
      this->get_node_waitables_interface()->add_waitable(sub->get_waitable(), options.callback_group);
    }
    return sub;
  }

  /**
   * @brief Create a FlatSubscription without callback for pure WaitSet or Polling pattern.
   *
   * @tparam ProtoMsgT The C++ Protobuf message class.
   * @tparam RosMsgT The ROS 2 message struct class (defaults to ProtoMsgT).
   * @param topic_name The topic name to subscribe to.
   * @param qos Quality of Service settings.
   * @param mode Subscription mode (e.g., FlatSubscriptionMode::WaitSetPull).
   * @param options Subscription options.
   * @return Shared pointer to the created FlatSubscription.
   */
  template <typename ProtoMsgT, typename RosMsgT = ProtoMsgT>
  std::shared_ptr<FlatSubscription<ProtoMsgT, RosMsgT>> create_flat_subscription(
    const std::string & topic_name, const rclcpp::QoS & qos, FlatSubscriptionMode mode,
    const rclcpp::SubscriptionOptions & options = rclcpp::SubscriptionOptions())
  {
    (void)qos;
    std::function<void(const ProtoMsgT &)> empty_cb = nullptr;
    auto sub = std::make_shared<FlatSubscription<ProtoMsgT, RosMsgT>>(topic_name, empty_cb, mode);
    if (options.callback_group != nullptr) {
      this->get_node_waitables_interface()->add_waitable(sub->get_waitable(), options.callback_group);
    }
    return sub;
  }
};

}  // namespace protoros2

#endif  // PROTOROS2__ENTERPRISE_NODE_HPP_
