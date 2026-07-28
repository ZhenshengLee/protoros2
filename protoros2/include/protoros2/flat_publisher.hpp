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

#ifndef PROTOROS2__FLAT_PUBLISHER_HPP_
#define PROTOROS2__FLAT_PUBLISHER_HPP_

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "google/protobuf/message.h"
#include "protoros2/flat_backend.hpp"
#include "protoros2/posh_runtime_helper.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp/serialized_message.hpp"
#include "rclcpp/type_adapter.hpp"
#include "rmw/rmw.h"

namespace protoros2
{

/**
 * @brief FlatPublisher represents the high-performance bypass communication channel
 * for zero-copy / shared memory transmission based on Iceoryx and FlatBuffers.
 *
 * FlatPublisher bypasses multi-layered rclcpp serialization overhead by utilizing
 * native Iceoryx untyped publishers and FlatBuffers zero-copy packaging.
 *
 * @tparam ProtoMsgT The C++ Protobuf message class.
 * @tparam RosMsgT The ROS 2 message struct class (defaults to ProtoMsgT).
 */
template <typename ProtoMsgT, typename RosMsgT = ProtoMsgT>
class FlatPublisher
{
public:
  using SharedPtr = std::shared_ptr<FlatPublisher<ProtoMsgT, RosMsgT>>;

  explicit FlatPublisher(const std::string & topic_name) : topic_name_(topic_name)
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

    backend_ = details::create_flat_publisher_backend(topic_name_);
    if (backend_) {
      backend_->offer();
    }
  }

  ~FlatPublisher()
  {
    if (backend_) {
      backend_->stop_offer();
    }
  }

  /**
   * @brief Publish a Protobuf message instance over the Iceoryx + FlatBuffers bypass channel.
   *
   * @param proto_msg The Protobuf message to publish.
   */
  void publish(const ProtoMsgT & proto_msg)
  {
    if (!backend_) {
      throw std::runtime_error("FlatPublisher underlying backend is null");
    }

#if defined(PROTO_SSOT_ONLY)
    backend_->publish_protobuf(proto_msg);
#else
    if constexpr (std::is_base_of_v<google::protobuf::Message, ProtoMsgT>) {
      backend_->publish_protobuf(proto_msg);
    } else {
      static_assert(
        std::is_trivially_copyable_v<ProtoMsgT>,
        "FlatPublisher only supports Protobuf messages or trivially copyable POD types! TypeAdapter mapping for "
        "complex types is not supported over Flat channel.");
      backend_->publish_raw(&proto_msg, sizeof(ProtoMsgT));
    }
#endif
  }

  /// Get the raw underlying rclcpp::Publisher instance (always null for pure Flat/Iceoryx bypass channel).
  typename rclcpp::Publisher<RosMsgT>::SharedPtr get_raw_publisher() const { return nullptr; }

  /// Get the underlying publisher as PublisherBase (always null for pure Flat/Iceoryx bypass channel).
  rclcpp::PublisherBase::SharedPtr get_publisher_base() const { return nullptr; }

  /// Get the native Iceoryx UntypedPublisher instance.
  iox::popo::UntypedPublisher * get_iox_publisher() const { return backend_ ? backend_->get_iox_publisher() : nullptr; }

  /// Check whether the current RMW is running in native Protobuf mode or PROTO_SSOT_ONLY.
  bool is_protobuf_native() const { return is_protobuf_native_; }

  /// Check whether this publisher is operating as a high-performance bypass / flat channel.
  bool is_bypass_channel() const { return true; }

  const char * get_topic_name() const
  {
    if (topic_name_.empty()) {
      return "";
    }
    return topic_name_.c_str();
  }

  /// Get subscription count on this topic (using Iceoryx subscriber awareness).
  size_t get_subscription_count() const { return 0; }

private:
  std::string topic_name_;
  bool is_protobuf_native_{false};
  std::unique_ptr<details::FlatPublisherBackend> backend_;
};

}  // namespace protoros2

#endif  // PROTOROS2__FLAT_PUBLISHER_HPP_
