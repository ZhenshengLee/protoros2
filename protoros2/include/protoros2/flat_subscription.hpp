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

#ifndef PROTOROS2__FLAT_SUBSCRIPTION_HPP_
#define PROTOROS2__FLAT_SUBSCRIPTION_HPP_

#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "google/protobuf/message.h"
#include "protoros2/flat_backend.hpp"
#include "protoros2/posh_runtime_helper.hpp"
#include "rclcpp/rclcpp.hpp"

namespace protoros2
{

/**
 * @brief FlatSubscription represents the bypass zero-copy receiving channel
 * corresponding to FlatPublisher based on Iceoryx and FlatBuffers zero-copy reception.
 *
 * @tparam ProtoMsgT The C++ Protobuf message class.
 * @tparam RosMsgT The ROS 2 message struct class (defaults to ProtoMsgT).
 */
template <typename ProtoMsgT, typename RosMsgT = ProtoMsgT>
class FlatSubscription
{
public:
  using SharedPtr = std::shared_ptr<FlatSubscription<ProtoMsgT, RosMsgT>>;

  FlatSubscription(const std::string & topic_name, std::function<void(const ProtoMsgT &)> callback)
  : topic_name_(topic_name), callback_(std::move(callback))
  {
    if (topic_name_.empty() || topic_name_[0] != '/') {
      topic_name_ = "/" + topic_name_;
    }

    const char * rmw_format = rmw_get_serialization_format();
    is_protobuf_native_ = (rmw_format != nullptr && std::string(rmw_format) == "protobuf");
#if defined(PROTO_SSOT_ONLY)
    is_protobuf_native_ = true;
#endif

    ensure_posh_runtime_initialized();

    backend_ = details::create_flat_subscription_backend(
      topic_name_, [this](const void * payload, size_t size, bool is_flatbuffer_vec) {
        if (!payload || size == 0 || !this->callback_) {
          return;
        }
#if defined(PROTO_SSOT_ONLY)
        if (is_flatbuffer_vec) {
          google::protobuf::io::ArrayInputStream raw_input(payload, static_cast<int>(size));
          ProtoMsgT proto_msg;
          if (proto_msg.ParseFromZeroCopyStream(&raw_input)) {
            this->callback_(proto_msg);
          } else {
            RCLCPP_ERROR(
              rclcpp::get_logger("FlatSubscription"),
              "Failed to parse Protobuf message from ZeroCopyStream (payload size: %zu)", size);
          }
        }
#else
        if constexpr (std::is_base_of_v<google::protobuf::Message, ProtoMsgT>) {
          if (is_flatbuffer_vec) {
            google::protobuf::io::ArrayInputStream raw_input(payload, static_cast<int>(size));
            ProtoMsgT proto_msg;
            if (proto_msg.ParseFromZeroCopyStream(&raw_input)) {
              this->callback_(proto_msg);
            } else {
              RCLCPP_ERROR(
                rclcpp::get_logger("FlatSubscription"),
                "Failed to parse Protobuf message from ZeroCopyStream (payload size: %zu)", size);
            }
          }
        } else {
          if (sizeof(ProtoMsgT) <= size && !is_flatbuffer_vec) {
            ProtoMsgT msg;
            std::memcpy(&msg, payload, sizeof(ProtoMsgT));
            this->callback_(msg);
          }
        }
#endif
      });

    if (backend_) {
      backend_->subscribe();
    }
  }

  ~FlatSubscription()
  {
    if (backend_) {
      backend_->unsubscribe();
    }
  }

  /// Get the underlying SubscriptionBase pointer (always null for pure Flat/Iceoryx bypass channel).
  std::shared_ptr<rclcpp::SubscriptionBase> get_subscription_base() const { return nullptr; }

  /// Get the raw underlying rclcpp::Subscription instance (always null for pure Flat/Iceoryx bypass channel).
  std::shared_ptr<void> get_raw_subscription() const { return nullptr; }

  /// Get the underlying Iceoryx UntypedSubscriber instance for advanced bypass inspections.
  iox::popo::UntypedSubscriber * get_iox_subscriber() const
  {
    return backend_ ? backend_->get_iox_subscriber() : nullptr;
  }

  const char * get_topic_name() const
  {
    if (topic_name_.empty()) {
      return "";
    }
    return topic_name_.c_str();
  }

  bool is_bypass_channel() const { return true; }

  bool is_protobuf_native() const { return is_protobuf_native_; }

private:
  std::string topic_name_;
  bool is_protobuf_native_{false};
  std::function<void(const ProtoMsgT &)> callback_;
  std::unique_ptr<details::FlatSubscriptionBackend> backend_;
};

}  // namespace protoros2

#endif  // PROTOROS2__FLAT_SUBSCRIPTION_HPP_
