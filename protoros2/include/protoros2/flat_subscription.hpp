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

  FlatSubscriptionWaitable() = default;

  // Bound by the factory once the owning FlatSubscription is managed by a shared_ptr
  // (weak_from_this() is not usable inside the subscription constructor).
  void set_subscription(std::weak_ptr<FlatSubscription<ProtoMsgT, RosMsgT>> subscription)
  {
    subscription_ = std::move(subscription);
  }

  size_t get_number_of_ready_guard_conditions() override
  {
    auto sub = subscription_.lock();
    return (sub && sub->get_guard_condition()) ? 1 : 0;
  }

  void add_to_wait_set(rcl_wait_set_t & wait_set) override
  {
    auto sub = subscription_.lock();
    if (!sub) {
      return;
    }
    auto gc = sub->get_guard_condition();
    if (!gc) {
      return;
    }
    size_t index;
    rcl_wait_set_add_guard_condition(&wait_set, &gc->get_rcl_guard_condition(), &index);
  }

  bool is_ready(const rcl_wait_set_t & wait_set) override
  {
    (void)wait_set;
    auto sub = subscription_.lock();
    return sub && sub->has_data();
  }

  std::shared_ptr<void> take_data() override
  {
    auto sub = subscription_.lock();
    if (!sub) {
      return nullptr;
    }
    auto msg_ptr = std::make_shared<ProtoMsgT>();
    if (sub->take(*msg_ptr)) {
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
    auto sub = subscription_.lock();
    if (data && sub && sub->get_callback()) {
      auto msg_ptr = std::static_pointer_cast<ProtoMsgT>(const_cast<std::shared_ptr<void> &>(data));
      sub->get_callback()(*msg_ptr);
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
  std::weak_ptr<FlatSubscription<ProtoMsgT, RosMsgT>> subscription_;
  std::function<void(size_t, int)> on_ready_callback_;
};

/**
 * @brief FlatSubscription represents the bypass zero-copy receiving channel
 * corresponding to FlatPublisher, based on iceoryx2 shared memory zero-copy reception.
 *
 * It supports both callback push mode (high performance direct dispatch) and
 * WaitSet / Polling / CallbackGroup executor paradigms via rclcpp::Waitable bridge.
 *
 * Thread contract (CallbackPush mode): the user callback is invoked on the backend's
 * internal poll thread, NOT on an executor thread (this is the deliberate low-latency
 * design of the bypass channel). The callback must be non-blocking and must not call
 * node/subscription lifecycle APIs (create/destroy/reset of nodes or subscriptions),
 * otherwise deadlocks or races may occur. Use WaitSetPull + CallbackGroup when executor
 * threading semantics are required.
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
    std::shared_ptr<details::EnterpriseNodeBackend> node_backend, const std::string & topic_name,
    std::function<void(const ProtoMsgT &)> callback, FlatSubscriptionMode mode = FlatSubscriptionMode::CallbackPush)
  : topic_name_(topic_name), mode_(mode), callback_(std::move(callback))
  {
    if (topic_name_.empty() || topic_name_[0] != '/') {
      topic_name_ = "/" + topic_name_;
    }

    const char * rmw_format = rmw_get_serialization_format();
    is_protobuf_native_ = (rmw_format != nullptr && std::string(rmw_format) == "protobuf");

    waitable_ = std::make_shared<FlatSubscriptionWaitable<ProtoMsgT, RosMsgT>>();
    waitable_ptr_ = std::shared_ptr<rclcpp::Waitable>(waitable_.get(), [keep_alive = waitable_](rclcpp::Waitable * w) {
      if (w) {
        w->exchange_in_use_by_wait_set_state(false);
      }
    });

    backend_ = details::create_flat_subscription_backend(
      node_backend, topic_name_, mode_, [this](const void * payload, size_t size) {
        if (!payload || size == 0) {
          return;
        }
        if constexpr (std::is_base_of_v<google::protobuf::Message, ProtoMsgT>) {
          this->cached_msg_.Clear();
          if (this->cached_msg_.ParseFromArray(payload, static_cast<int>(size))) {
            if (this->callback_) {
              this->callback_(this->cached_msg_);
            }
          } else {
            RCLCPP_ERROR(
              rclcpp::get_logger("FlatSubscription"),
              "Failed to parse Protobuf message from flat payload (payload size: %zu)", size);
          }
        } else {
          static_assert(
            std::is_trivially_copyable_v<ProtoMsgT>,
            "FlatSubscription only supports Protobuf messages or trivially copyable POD types! "
            "TypeAdapter mapping for complex types is not supported over Flat channel.");
          if (size == sizeof(ProtoMsgT)) {
            ProtoMsgT msg;
            std::memcpy(&msg, payload, sizeof(ProtoMsgT));
            if (this->callback_) {
              this->callback_(msg);
            }
          } else {
            RCLCPP_ERROR(
              rclcpp::get_logger("FlatSubscription"),
              "Flat payload size %zu does not match POD type size %zu, dropping (publisher/subscriber type "
              "mismatch?)",
              size, sizeof(ProtoMsgT));
          }
        }
        if (waitable_) {
          waitable_->trigger_ready();
        }
      });

    if (backend_) {
      backend_->subscribe();
    }
  }

  /**
   * @internal Bind the waitable's weak back-reference. Must be called by the creating
   * factory (EnterpriseNode) immediately after this object is owned by a shared_ptr:
   * the executor may outlive the subscription and would otherwise dereference a dangling
   * pointer through the waitable. Weak binding makes such late access degrade to
   * not-ready/no-data instead of undefined behavior.
   */
  void bind_waitable_back_reference(const std::shared_ptr<FlatSubscription> & self)
  {
    if (waitable_) {
      waitable_->set_subscription(self);
    }
  }

  /**
   * @brief Take one message directly from the underlying Iceoryx queue without callback dispatch.
   * Useful in Polling Subscriber and WaitSet pull patterns.
   *
   * Latest-wins pull semantics: a single sample is taken per call; backlogged samples stay
   * queued and are drained by subsequent calls. Do not assume one call consumes everything.
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
    backend_->take_payload([&](const void * payload, size_t size) {
      if (!payload || size == 0) {
        return;
      }
      if constexpr (std::is_base_of_v<google::protobuf::Message, ProtoMsgT>) {
        out_msg.Clear();
        taken = out_msg.ParseFromArray(payload, static_cast<int>(size));
      } else {
        static_assert(
          std::is_trivially_copyable_v<ProtoMsgT>,
          "FlatSubscription only supports Protobuf messages or trivially copyable POD types! TypeAdapter mapping for "
          "complex types is not supported over Flat channel.");
        if (size == sizeof(ProtoMsgT)) {
          std::memcpy(&out_msg, payload, sizeof(ProtoMsgT));
          taken = true;
        }
      }
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
    wait_set.remove_guard_condition(gc);
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
  std::shared_ptr<rclcpp::Waitable> get_waitable() const { return waitable_ptr_; }

  const std::function<void(const ProtoMsgT &)> & get_callback() const { return callback_; }

  /// Get the underlying SubscriptionBase pointer (always null for pure Flat/Iceoryx bypass channel).
  std::shared_ptr<rclcpp::SubscriptionBase> get_subscription_base() const { return nullptr; }

  /// Get the raw underlying rclcpp::Subscription instance (always null for pure Flat/Iceoryx bypass channel).
  std::shared_ptr<void> get_raw_subscription() const { return nullptr; }

  /// Get the backend handle for advanced bypass inspections.
  void * get_backend_handle() const { return backend_ ? backend_->get_backend_handle() : nullptr; }

  const char * get_topic_name() const
  {
    if (topic_name_.empty()) {
      return "";
    }
    return topic_name_.c_str();
  }

  bool is_bypass_channel() const { return true; }

  /// True when the bypass channel was fully established; false means no data will ever arrive.
  bool is_valid() const { return backend_ && backend_->valid(); }

  bool is_protobuf_native() const { return is_protobuf_native_; }

private:
  std::string topic_name_;
  FlatSubscriptionMode mode_;
  bool is_protobuf_native_{false};
  std::function<void(const ProtoMsgT &)> callback_;
  ProtoMsgT cached_msg_;
  std::shared_ptr<details::FlatSubscriptionBackend> backend_;
  std::shared_ptr<FlatSubscriptionWaitable<ProtoMsgT, RosMsgT>> waitable_;
  std::shared_ptr<rclcpp::Waitable> waitable_ptr_;
};

}  // namespace protoros2

#endif  // PROTOROS2__FLAT_SUBSCRIPTION_HPP_
