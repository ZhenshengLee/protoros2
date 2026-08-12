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

#include <chrono>
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
#include "rclcpp/guard_condition.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp/waitable.hpp"

namespace protoros2
{

template <typename ProtoMsgT, typename RosMsgT>
class FlatSubscription;

template <typename ProtoMsgT, typename RosMsgT = ProtoMsgT>
class FlatSubscriptionWaitable : public rclcpp::Waitable
{
public:
  using SharedPtr = std::shared_ptr<FlatSubscriptionWaitable<ProtoMsgT, RosMsgT>>;

  explicit FlatSubscriptionWaitable(FlatSubscription<ProtoMsgT, RosMsgT> * subscription) : subscription_(subscription)
  {
  }

  size_t get_number_of_ready_guard_conditions() override
  {
    return (subscription_ && subscription_->get_guard_condition()) ? 1 : 0;
  }

  void add_to_wait_set(rcl_wait_set_t & wait_set) override
  {
    if (!subscription_) {
      return;
    }
    auto gc = subscription_->get_guard_condition();
    if (!gc) {
      return;
    }
    size_t index;
    rcl_wait_set_add_guard_condition(&wait_set, &gc->get_rcl_guard_condition(), &index);
  }

  bool is_ready(const rcl_wait_set_t & wait_set) override
  {
    (void)wait_set;
    return subscription_ && subscription_->has_data();
  }

  std::shared_ptr<void> take_data() override
  {
    if (!subscription_) {
      return nullptr;
    }
    auto msg_ptr = std::make_shared<ProtoMsgT>();
    if (subscription_->take(*msg_ptr)) {
      return msg_ptr;
    }
    return nullptr;
  }

  std::shared_ptr<void> take_data_by_entity_id(size_t id) override
  {
    (void)id;
    return take_data();
  }

  void execute(const std::shared_ptr<void> & data) override
  {
    if (data && subscription_ && subscription_->get_callback()) {
      auto msg_ptr = std::static_pointer_cast<ProtoMsgT>(const_cast<std::shared_ptr<void> &>(data));
      subscription_->get_callback()(*msg_ptr);
    }
  }

  std::vector<std::shared_ptr<rclcpp::TimerBase>> get_timers() const override { return {}; }

  void set_on_ready_callback(std::function<void(size_t, int)> callback) override
  {
    on_ready_callback_ = std::move(callback);
  }

  void clear_on_ready_callback() override { on_ready_callback_ = nullptr; }

  void trigger_ready()
  {
    if (on_ready_callback_) {
      on_ready_callback_(1, 0);
    }
  }

private:
  FlatSubscription<ProtoMsgT, RosMsgT> * subscription_{nullptr};
  std::function<void(size_t, int)> on_ready_callback_;
};

/**
 * @brief FlatSubscription represents the bypass zero-copy receiving channel
 * corresponding to FlatPublisher based on Iceoryx and FlatBuffers zero-copy reception.
 *
 * It supports both callback push mode (high performance direct dispatch) and
 * WaitSet / Polling / CallbackGroup executor paradigms via rclcpp::Waitable bridge.
 *
 * @tparam ProtoMsgT The C++ Protobuf message class.
 * @tparam RosMsgT The ROS 2 message struct class (defaults to ProtoMsgT).
 */
template <typename ProtoMsgT, typename RosMsgT = ProtoMsgT>
class FlatSubscription
{
public:
  using SharedPtr = std::shared_ptr<FlatSubscription<ProtoMsgT, RosMsgT>>;

  FlatSubscription(
    const std::string & topic_name, std::function<void(const ProtoMsgT &)> callback,
    FlatSubscriptionMode mode = FlatSubscriptionMode::CallbackPush)
  : topic_name_(topic_name), mode_(mode), callback_(std::move(callback))
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

    waitable_ = std::make_shared<FlatSubscriptionWaitable<ProtoMsgT, RosMsgT>>(this);

    backend_ = details::create_flat_subscription_backend(
      topic_name_, mode_, [this](const void * payload, size_t size, bool is_flatbuffer_vec) {
        if (!payload || size == 0) {
          return;
        }
#if defined(PROTO_SSOT_ONLY)
        if (is_flatbuffer_vec) {
          google::protobuf::io::ArrayInputStream raw_input(payload, static_cast<int>(size));
          google::protobuf::Arena arena;
          auto * msg = google::protobuf::Arena::CreateMessage<ProtoMsgT>(&arena);
          if (msg->ParseFromZeroCopyStream(&raw_input)) {
            if (this->callback_) {
              this->callback_(*msg);
            }
          } else {
            RCLCPP_ERROR(
              rclcpp::get_logger("FlatSubscription"),
              "Failed to parse Protobuf message from ZeroCopyStream (payload size: %zu)", size);
          }
        }
#else
        if constexpr (std::is_base_of_v<google::protobuf::Message, ProtoMsgT>) {
          // Parse directly from payload whether it was wrapped in flatbuffer or not.
          // In the raw protobuf mode, is_flatbuffer_vec is false but we still parse the raw bytes.
          google::protobuf::io::ArrayInputStream raw_input(payload, static_cast<int>(size));
          google::protobuf::Arena arena;
          auto* msg = google::protobuf::Arena::CreateMessage<ProtoMsgT>(&arena);
          if (msg->ParseFromZeroCopyStream(&raw_input)) {
            if (this->callback_) {
              this->callback_(*msg);
            }
          } else {
            RCLCPP_ERROR(
              rclcpp::get_logger("FlatSubscription"),
              "Failed to parse Protobuf message from ZeroCopyStream (payload size: %zu)", size);
          }
        } else {
          static_assert(std::is_trivially_copyable_v<ProtoMsgT>,
            "FlatSubscription only supports Protobuf messages or trivially copyable POD types! "
            "TypeAdapter mapping for complex types is not supported over Flat channel.");
          if (sizeof(ProtoMsgT) <= size && !is_flatbuffer_vec) {
            ProtoMsgT msg;
            std::memcpy(&msg, payload, sizeof(ProtoMsgT));
            if (this->callback_) {
              this->callback_(msg);
            }
          }
        }
#endif
        if (waitable_) {
          waitable_->trigger_ready();
        }
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

  /**
   * @brief Take a message directly from the underlying Iceoryx queue without callback dispatch.
   * Useful in Polling Subscriber and WaitSet pull patterns.
   *
   * @param out_msg Reference to the message to populate.
   * @return true if a message was taken successfully, false otherwise.
   */
  bool take(ProtoMsgT & out_msg)
  {
    if (!backend_) {
      return false;
    }
    bool taken = false;
    backend_->take_payload([&](const void * payload, size_t size, bool is_flatbuffer_vec) {
      if (!payload || size == 0) {
        return;
      }
#if defined(PROTO_SSOT_ONLY)
      if (is_flatbuffer_vec) {
        google::protobuf::io::ArrayInputStream raw_input(payload, static_cast<int>(size));
        out_msg.Clear();
        taken = out_msg.ParseFromZeroCopyStream(&raw_input);
      }
#else
      if constexpr (std::is_base_of_v<google::protobuf::Message, ProtoMsgT>) {
        google::protobuf::io::ArrayInputStream raw_input(payload, static_cast<int>(size));
        out_msg.Clear();
        taken = out_msg.ParseFromZeroCopyStream(&raw_input);
      } else {
        static_assert(
          std::is_trivially_copyable_v<ProtoMsgT>,
          "FlatSubscription only supports Protobuf messages or trivially copyable POD types! TypeAdapter mapping for "
          "complex types is not supported over Flat channel.");
        if (sizeof(ProtoMsgT) <= size && !is_flatbuffer_vec) {
          std::memcpy(&out_msg, payload, sizeof(ProtoMsgT));
          taken = true;
        }
      }
#endif
    });
    return taken;
  }

  /**
   * @brief Check whether there is pending data available in the subscription queue.
   */
  bool has_data() const { return backend_ && backend_->has_data(); }

  /**
   * @brief Wait for data to become available up to a specified timeout.
   * Internally creates a temporary WaitSet to block on the guard condition,
   * keeping the WaitSet complexity completely transparent to the user.
   *
   * @param timeout The maximum duration to wait.
   * @return true if data became ready within the timeout, false otherwise.
   */
  bool wait_for_data(std::chrono::nanoseconds timeout)
  {
    if (!backend_) {
      return false;
    }
    auto gc = backend_->get_guard_condition();
    if (!gc) {
      return false;
    }
    rclcpp::WaitSet wait_set;
    wait_set.add_guard_condition(gc);
    auto result = wait_set.wait(timeout);
    return result.kind() == rclcpp::WaitResultKind::Ready;
  }

  /**
   * @brief Get the underlying GuardCondition for rclcpp::WaitSet registration.
   */
  rclcpp::GuardCondition::SharedPtr get_guard_condition() const
  {
    return backend_ ? backend_->get_guard_condition() : nullptr;
  }

  /**
   * @brief Get the rclcpp::Waitable bridge for Executor and CallbackGroup registration.
   * Utilizes a custom deleter to automatically clear the 'in_use_by_wait_set_' state
   * when the returned shared_ptr is destroyed (e.g., when the WaitSet is destroyed),
   * overcoming the native WaitSet lifecycle tracking limitation.
   */
  std::shared_ptr<rclcpp::Waitable> get_waitable() const
  {
    if (!waitable_) {
      return nullptr;
    }
    return std::shared_ptr<rclcpp::Waitable>(waitable_.get(), [keep_alive = waitable_](rclcpp::Waitable * w) {
      if (w) {
        w->exchange_in_use_by_wait_set_state(false);
      }
    });
  }

  const std::function<void(const ProtoMsgT &)> & get_callback() const { return callback_; }

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
  FlatSubscriptionMode mode_;
  bool is_protobuf_native_{false};
  std::function<void(const ProtoMsgT &)> callback_;
  std::unique_ptr<details::FlatSubscriptionBackend> backend_;
  std::shared_ptr<FlatSubscriptionWaitable<ProtoMsgT, RosMsgT>> waitable_;
};

}  // namespace protoros2

#endif  // PROTOROS2__FLAT_SUBSCRIPTION_HPP_
