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

#include "protoros2/flat_backend.hpp"

#include <cstring>
#include <stdexcept>
#include <utility>
#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <algorithm>

#include "iox2/node.hpp"
#include "iox2/service_name.hpp"
#include "iox2/service_type.hpp"
#include "iox2/publisher.hpp"
#include "iox2/subscriber.hpp"
#include "iox2/sample_mut_uninit.hpp"
#include "iox2/sample.hpp"
#include "iox2/listener.hpp"
#include "iox2/notifier.hpp"
#include "iox2/signal_handling_mode.hpp"
#include "iox2/waitset.hpp"
#include "iox2/iceoryx2.hpp"
#include "iox2/bb/slice.hpp"
#include "rclcpp/guard_condition.hpp"
#include "rclcpp/rclcpp.hpp"

namespace protoros2
{
namespace details
{

class FlatSubscriptionBackendImpl;

class EnterpriseNodeBackendImpl : public EnterpriseNodeBackend
{
public:
  explicit EnterpriseNodeBackendImpl(const std::string & node_name)
  {
    std::string safe_name = node_name;
    std::replace(safe_name.begin(), safe_name.end(), '/', '_');
    if (safe_name.empty() || safe_name[0] == '_') {
      safe_name = "node" + safe_name;
    }
    auto iox_node_name = iox2::NodeName::create(safe_name.c_str());
    if (iox_node_name.has_value()) {
      auto node_res = iox2::NodeBuilder().name(iox_node_name.value()).create<iox2::ServiceType::Ipc>();
      if (node_res.has_value()) {
        node_.emplace(std::move(node_res.value()));
      }
    } else {
      auto fallback_name = iox2::NodeName::create("protoros2_node");
      auto node_res = iox2::NodeBuilder().name(fallback_name.value()).create<iox2::ServiceType::Ipc>();
      if (node_res.has_value()) {
        node_.emplace(std::move(node_res.value()));
      }
    }

    // Keep iceoryx2 from installing its own SIGINT/SIGTERM handlers: the default
    // HandleTerminationRequests mode overwrites the rclcpp signal handler via sigaction
    // without chaining, so Ctrl-C would never trigger rclcpp shutdown and spin() would hang.
    auto waitset_res =
      iox2::WaitSetBuilder().signal_handling_mode(iox2::SignalHandlingMode::Disabled).create<iox2::ServiceType::Ipc>();
    if (waitset_res.has_value()) {
      waitset_.emplace(std::move(waitset_res.value()));
    }
  }

  ~EnterpriseNodeBackendImpl() override { stop_waitset_thread(); }

  bool valid() const override { return node_.has_value() && waitset_.has_value(); }

  iox2::Node<iox2::ServiceType::Ipc> * get_node() { return node_.has_value() ? &node_.value() : nullptr; }

  void register_listener(iox2::Listener<iox2::ServiceType::Ipc> & listener, std::function<void()> on_data)
  {
    if (!waitset_.has_value()) return;
    auto attach_res = waitset_->attach_notification(listener);
    if (attach_res.has_value()) {
      std::lock_guard<std::mutex> lock(map_mutex_);
      attachment_map_.push_back(Attachment{std::move(attach_res.value()), std::move(on_data), &listener});
      start_waitset_thread_if_needed();
    }
  }

  // Detach the listener's WaitSetGuard while the listener is still alive; the guard destructor
  // removes the (already closed otherwise) fd from epoll, so it must run before listener teardown.
  void unregister_listener(iox2::Listener<iox2::ServiceType::Ipc> & listener)
  {
    std::lock_guard<std::mutex> lock(map_mutex_);
    auto it = std::find_if(attachment_map_.begin(), attachment_map_.end(), [&listener](const Attachment & attachment) {
      return attachment.listener == &listener;
    });
    if (it != attachment_map_.end()) {
      attachment_map_.erase(it);
    }
  }

private:
  void start_waitset_thread_if_needed()
  {
    if (!poll_thread_.joinable()) {
      stop_polling_ = false;
      poll_thread_ = std::thread([this]() {
        while (!this->stop_polling_) {
          if (!waitset_.has_value()) break;
          if (!rclcpp::ok()) {
            std::vector<std::function<void()>> tasks;
            {
              std::lock_guard<std::mutex> lock(this->map_mutex_);
              tasks.reserve(this->attachment_map_.size());
              for (auto const & attachment : this->attachment_map_) {
                tasks.push_back(attachment.on_data);
              }
            }
            for (auto const & task : tasks) {
              task();  // trigger to wake up executor
            }
            break;
          }
          auto wait_res = waitset_->wait_and_process_once_with_timeout(
            [this](iox2::WaitSetAttachmentId<iox2::ServiceType::Ipc> id) {
              // Route precisely: only the subscription whose listener fired is dispatched.
              // The callback is copied under the lock and executed outside of it, so user
              // callbacks neither block (un)registration nor deadlock on re-entrant unregister.
              std::function<void()> task;
              {
                std::lock_guard<std::mutex> lock(this->map_mutex_);
                auto it = std::find_if(
                  this->attachment_map_.begin(), this->attachment_map_.end(),
                  [&id](const Attachment & attachment) { return id.has_event_from(attachment.guard); });
                if (it != this->attachment_map_.end()) {
                  task = it->on_data;
                }
              }
              if (task) {
                task();
              }
              return iox2::CallbackProgression::Continue;
            },
            iox2::bb::Duration::from_millis(50));
        }
      });
    }
  }

  void stop_waitset_thread()
  {
    stop_polling_ = true;
    if (poll_thread_.joinable()) {
      poll_thread_.join();
    }
  }

  struct Attachment
  {
    iox2::WaitSetGuard<iox2::ServiceType::Ipc> guard;
    std::function<void()> on_data;
    iox2::Listener<iox2::ServiceType::Ipc> * listener;
  };

  iox2::bb::Optional<iox2::Node<iox2::ServiceType::Ipc>> node_;
  iox2::bb::Optional<iox2::WaitSet<iox2::ServiceType::Ipc>> waitset_;
  std::vector<Attachment> attachment_map_;
  std::mutex map_mutex_;
  std::atomic<bool> stop_polling_{false};
  std::thread poll_thread_;
};

std::shared_ptr<EnterpriseNodeBackend> create_enterprise_node_backend(const std::string & node_name)
{
  return std::make_shared<EnterpriseNodeBackendImpl>(node_name);
}

class FlatPublisherBackendImpl : public FlatPublisherBackend
{
public:
  explicit FlatPublisherBackendImpl(
    std::shared_ptr<EnterpriseNodeBackend> node_backend, const std::string & topic_name,
    size_t max_payload_bytes = kDefaultMaxFlatPayloadBytes)
  : node_backend_(node_backend), topic_name_(topic_name)
  {
    std::string clean_topic = topic_name_.empty() ? "" : (topic_name_[0] == '/' ? topic_name_.substr(1) : topic_name_);
    std::replace(clean_topic.begin(), clean_topic.end(), '/', '_');

    auto backend_impl = std::dynamic_pointer_cast<EnterpriseNodeBackendImpl>(node_backend_);
    if (backend_impl && backend_impl->get_node()) {
      auto node = backend_impl->get_node();
      auto service_name = iox2::ServiceName::create(clean_topic.c_str());
      if (service_name.has_value()) {
        auto service =
          node->service_builder(service_name.value()).publish_subscribe<iox2::bb::Slice<uint8_t>>().open_or_create();
        if (service.has_value()) {
          // Payloads larger than the negotiated slice are dropped (see publish_fail_count).
          auto pub = service->publisher_builder().initial_max_slice_len(max_payload_bytes).create();
          if (pub.has_value()) publisher_.emplace(std::move(pub.value()));
        }
      }

      auto event_name = iox2::ServiceName::create((clean_topic + "_evt").c_str());
      if (event_name.has_value()) {
        auto event_service = node->service_builder(event_name.value()).event().open_or_create();
        if (event_service.has_value()) {
          auto notif = event_service->notifier_builder().create();
          if (notif.has_value()) notifier_.emplace(std::move(notif.value()));
        }
      }
    }
  }

  void publish_protobuf(const google::protobuf::Message & proto_msg) override
  {
    if (!publisher_.has_value()) {
      throw std::runtime_error("FlatPublisher underlying iceoryx2 publisher is null");
    }

    size_t pb_size = proto_msg.ByteSizeLong();

    auto loan_result = publisher_->loan_slice_uninit(pb_size);
    if (!loan_result.has_value()) {
      publish_fail_count_++;
      RCLCPP_ERROR(rclcpp::get_logger("FlatPublisher"), "Failed to loan chunk from iceoryx2");
      return;
    }

    auto sample = std::move(loan_result.value());
    // Serialize the Protobuf payload directly into shared memory: the buffer IS the message
    // (zero framing), the receiver gets the exact size from the iceoryx2 sample.
    if (!proto_msg.SerializeToArray(sample.payload_mut().data(), static_cast<int>(pb_size))) {
      publish_fail_count_++;
      RCLCPP_ERROR(rclcpp::get_logger("FlatPublisher"), "SerializeToArray failed, dropping message");
      return;  // the loaned sample is released without being sent
    }

    auto send_res = iox2::send(iox2::assume_init(std::move(sample)));
    if (!send_res.has_value()) {
      publish_fail_count_++;
      RCLCPP_ERROR(rclcpp::get_logger("FlatPublisher"), "Failed to send chunk via iceoryx2");
    } else if (notifier_.has_value()) {
      (void)notifier_->notify();
    }
  }

  void publish_raw(const void * data, size_t size) override
  {
    if (!publisher_.has_value()) {
      throw std::runtime_error("FlatPublisher underlying iceoryx2 publisher is null");
    }

    auto loan_result = publisher_->loan_slice_uninit(size);
    if (!loan_result.has_value()) {
      publish_fail_count_++;
      RCLCPP_ERROR(rclcpp::get_logger("FlatPublisher"), "Failed to loan raw chunk from iceoryx2");
      return;
    }

    auto sample = std::move(loan_result.value());
    std::memcpy(sample.payload_mut().data(), data, size);

    auto send_res = iox2::send(iox2::assume_init(std::move(sample)));
    if (!send_res.has_value()) {
      publish_fail_count_++;
      RCLCPP_ERROR(rclcpp::get_logger("FlatPublisher"), "Failed to send raw chunk via iceoryx2");
    } else if (notifier_.has_value()) {
      (void)notifier_->notify();
    }
  }

  void * get_backend_handle() const override
  {
    return publisher_.has_value() ? const_cast<void *>(static_cast<const void *>(&publisher_.value())) : nullptr;
  }

  size_t publish_fail_count() const override { return publish_fail_count_.load(); }

  bool valid() const override { return publisher_.has_value() && notifier_.has_value(); }

private:
  std::shared_ptr<EnterpriseNodeBackend> node_backend_;
  std::string topic_name_;
  std::atomic<size_t> publish_fail_count_{0};
  iox2::bb::Optional<iox2::Publisher<iox2::ServiceType::Ipc, iox2::bb::Slice<uint8_t>, void>> publisher_;
  iox2::bb::Optional<iox2::Notifier<iox2::ServiceType::Ipc>> notifier_;
};

class FlatSubscriptionBackendImpl : public FlatSubscriptionBackend,
                                    public std::enable_shared_from_this<FlatSubscriptionBackendImpl>
{
public:
  FlatSubscriptionBackendImpl(
    std::shared_ptr<EnterpriseNodeBackend> node_backend, const std::string & topic_name, FlatSubscriptionMode mode,
    FlatPayloadCallback callback)
  : node_backend_(node_backend), topic_name_(topic_name), mode_(mode), callback_(std::move(callback))
  {
    std::string clean_topic = topic_name_.empty() ? "" : (topic_name_[0] == '/' ? topic_name_.substr(1) : topic_name_);
    std::replace(clean_topic.begin(), clean_topic.end(), '/', '_');

    auto backend_impl = std::dynamic_pointer_cast<EnterpriseNodeBackendImpl>(node_backend_);
    if (backend_impl && backend_impl->get_node()) {
      auto node = backend_impl->get_node();
      auto service_name = iox2::ServiceName::create(clean_topic.c_str());
      if (service_name.has_value()) {
        auto service =
          node->service_builder(service_name.value()).publish_subscribe<iox2::bb::Slice<uint8_t>>().open_or_create();
        if (service.has_value()) {
          auto sub = service->subscriber_builder().create();
          if (sub.has_value()) {
            subscriber_.emplace(std::move(sub.value()));
            RCLCPP_INFO(rclcpp::get_logger("Backend"), "Subscriber created successfully!");
          } else {
            RCLCPP_ERROR(
              rclcpp::get_logger("Backend"), "Failed to create subscriber! Error code: %d",
              static_cast<int>(sub.error()));
          }
        } else {
          RCLCPP_ERROR(
            rclcpp::get_logger("Backend"), "Failed to open_or_create service! Error code: %d",
            static_cast<int>(service.error()));
        }
      } else {
        RCLCPP_ERROR(rclcpp::get_logger("Backend"), "Failed to create ServiceName!");
      }

      auto event_name = iox2::ServiceName::create((clean_topic + "_evt").c_str());
      if (event_name.has_value()) {
        auto event_service = node->service_builder(event_name.value()).event().open_or_create();
        if (event_service.has_value()) {
          auto lst = event_service->listener_builder().create();
          if (lst.has_value()) {
            listener_.emplace(std::move(lst.value()));
            RCLCPP_INFO(rclcpp::get_logger("Backend"), "Listener created successfully!");
          } else {
            RCLCPP_ERROR(
              rclcpp::get_logger("Backend"), "Failed to create listener! Error code: %d",
              static_cast<int>(lst.error()));
          }
        } else {
          RCLCPP_ERROR(
            rclcpp::get_logger("Backend"), "Failed to open_or_create event service! Error code: %d",
            static_cast<int>(event_service.error()));
        }
      }
    }

    guard_condition_ = std::make_shared<rclcpp::GuardCondition>();
  }

  ~FlatSubscriptionBackendImpl() override
  {
    auto backend_impl = std::dynamic_pointer_cast<EnterpriseNodeBackendImpl>(node_backend_);
    if (backend_impl && listener_.has_value()) {
      backend_impl->unregister_listener(listener_.value());
    }
  }

  void subscribe() override
  {
    if (listener_.has_value()) {
      auto backend_impl = std::dynamic_pointer_cast<EnterpriseNodeBackendImpl>(node_backend_);
      if (backend_impl) {
        // weak capture: the poll thread may execute a copied callback after this backend
        // has been destroyed; lock() failing then safely drops the event.
        std::weak_ptr<FlatSubscriptionBackendImpl> weak_self = shared_from_this();
        backend_impl->register_listener(listener_.value(), [weak_self]() {
          if (auto self = weak_self.lock()) {
            self->on_data_received();
          }
        });
      }
    }
  }

  bool has_data() const override
  {
    if (!subscriber_.has_value()) return false;
    auto res = subscriber_->has_samples();
    return res.has_value() ? res.value() : false;
  }

  bool take_payload(FlatPayloadCallback processor) override
  {
    if (!subscriber_.has_value()) return false;

    auto receive_result = subscriber_->receive();
    if (!receive_result.has_value()) return false;

    auto maybe_sample = std::move(receive_result.value());
    if (!maybe_sample.has_value()) return false;

    auto sample = std::move(maybe_sample.value());
    const void * payload = sample.payload().data();
    size_t payload_size = sample.payload().number_of_elements();

    if (payload && payload_size > 0 && processor) {
      // Zero framing: the buffer is the message itself; the subscriber interprets it
      // according to its compile-time message type.
      processor(payload, payload_size);
    }
    return true;
  }

  rclcpp::GuardCondition::SharedPtr get_guard_condition() const override { return guard_condition_; }

  void * get_backend_handle() const override
  {
    return subscriber_.has_value() ? const_cast<void *>(static_cast<const void *>(&subscriber_.value())) : nullptr;
  }

  bool valid() const override { return subscriber_.has_value() && listener_.has_value(); }

private:
  void on_data_received() noexcept
  {
    if (listener_.has_value()) {
      (void)listener_->try_wait_all([](iox2::EventId) {});
    }

    if (guard_condition_) {
      guard_condition_->trigger();
    }

    if (mode_ == FlatSubscriptionMode::CallbackPush && callback_) {
      while (subscriber_.has_value()) {
        auto res = subscriber_->has_samples();
        if (res.has_value() && res.value()) {
          take_payload(callback_);
        } else {
          break;
        }
      }
    }
  }

  std::shared_ptr<EnterpriseNodeBackend> node_backend_;
  std::string topic_name_;
  FlatSubscriptionMode mode_;
  FlatPayloadCallback callback_;

  iox2::bb::Optional<iox2::Subscriber<iox2::ServiceType::Ipc, iox2::bb::Slice<uint8_t>, void>> subscriber_;
  iox2::bb::Optional<iox2::Listener<iox2::ServiceType::Ipc>> listener_;

  rclcpp::GuardCondition::SharedPtr guard_condition_;
};

std::unique_ptr<FlatPublisherBackend> create_flat_publisher_backend(
  std::shared_ptr<EnterpriseNodeBackend> node_backend, const std::string & topic_name, size_t max_payload_bytes)
{
  return std::make_unique<FlatPublisherBackendImpl>(node_backend, topic_name, max_payload_bytes);
}

std::shared_ptr<FlatSubscriptionBackend> create_flat_subscription_backend(
  std::shared_ptr<EnterpriseNodeBackend> node_backend, const std::string & topic_name, FlatSubscriptionMode mode,
  FlatPayloadCallback callback)
{
  return std::make_shared<FlatSubscriptionBackendImpl>(node_backend, topic_name, mode, std::move(callback));
}

}  // namespace details
}  // namespace protoros2
