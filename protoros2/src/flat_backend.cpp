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

#include "flatbuffers/flatbuffers.h"
#include "iceoryx_posh/mepoo/chunk_header.hpp"
#include "iceoryx_posh/popo/listener.hpp"
#include "iceoryx_posh/popo/untyped_publisher.hpp"
#include "iceoryx_posh/popo/untyped_subscriber.hpp"
#include "iceoryx_posh/runtime/posh_runtime.hpp"
#include "rclcpp/guard_condition.hpp"
#include "rclcpp/rclcpp.hpp"

namespace protoros2
{
namespace details
{

class FlatPublisherBackendImpl : public FlatPublisherBackend
{
public:
  explicit FlatPublisherBackendImpl(const std::string & topic_name) : topic_name_(topic_name)
  {
    std::string clean_topic = topic_name_.empty() ? "" : (topic_name_[0] == '/' ? topic_name_.substr(1) : topic_name_);
    iox_pub_ = std::make_unique<iox::popo::UntypedPublisher>(iox::capro::ServiceDescription(
      iox::capro::IdString_t(iox::cxx::TruncateToCapacity, "protoros2"),
      iox::capro::IdString_t(iox::cxx::TruncateToCapacity, clean_topic),
      iox::capro::IdString_t(iox::cxx::TruncateToCapacity, "flat")));
  }

  ~FlatPublisherBackendImpl() override { stop_offer(); }

  void offer() override
  {
    if (iox_pub_) {
      iox_pub_->offer();
    }
  }

  void stop_offer() override
  {
    if (iox_pub_) {
      iox_pub_->stopOffer();
    }
  }

  void publish_protobuf(const google::protobuf::Message & proto_msg) override
  {
    if (!iox_pub_) {
      throw std::runtime_error("FlatPublisher underlying Iceoryx publisher is null");
    }

    size_t pb_size = proto_msg.ByteSizeLong();
    // Ensure 4-byte boundary alignment for the payload section
    size_t aligned_pb_size = (pb_size + 3) & ~size_t(3);
    // 4 bytes Root Table Offset + 4 bytes Vector Length + aligned Protobuf payload
    size_t total_size = sizeof(uint32_t) + sizeof(uint32_t) + aligned_pb_size;

    auto loan_result = iox_pub_->loan(static_cast<uint32_t>(total_size));
    if (!loan_result.has_error()) {
      uint8_t * chunk = static_cast<uint8_t *>(loan_result.value());

      // 1. Write FlatBuffers Root Header: vector starts at byte offset 4
      uint32_t root_offset = sizeof(uint32_t);
      std::memcpy(chunk, &root_offset, sizeof(uint32_t));

      // 2. Write Vector Element Length: exact Protobuf byte count
      uint32_t vec_len = static_cast<uint32_t>(pb_size);
      std::memcpy(chunk + sizeof(uint32_t), &vec_len, sizeof(uint32_t));

      // 3. Direct zero-memcpy zero-heap serialization of Protobuf payload into Iceoryx shared memory
      proto_msg.SerializeToArray(chunk + sizeof(uint32_t) * 2, static_cast<int>(pb_size));

      // 4. Zero out any trailing padding bytes (0 to 3 bytes) to ensure deterministic memory alignment
      if (aligned_pb_size > pb_size) {
        std::memset(chunk + sizeof(uint32_t) * 2 + pb_size, 0, aligned_pb_size - pb_size);
      }

      iox_pub_->publish(chunk);
    } else {
      RCLCPP_ERROR(rclcpp::get_logger("FlatPublisher"), "Failed to loan chunk from Iceoryx shared memory");
    }
  }

  void publish_raw(const void * data, size_t size) override
  {
    if (!iox_pub_) {
      throw std::runtime_error("FlatPublisher underlying Iceoryx publisher is null");
    }

    auto loan_result = iox_pub_->loan(static_cast<uint32_t>(size));
    if (!loan_result.has_error()) {
      void * chunk = loan_result.value();
      std::memcpy(chunk, data, size);
      iox_pub_->publish(chunk);
    } else {
      RCLCPP_ERROR(rclcpp::get_logger("FlatPublisher"), "Failed to loan chunk from Iceoryx shared memory");
    }
  }

  iox::popo::UntypedPublisher * get_iox_publisher() const override { return iox_pub_.get(); }

private:
  std::string topic_name_;
  std::unique_ptr<iox::popo::UntypedPublisher> iox_pub_;
};

class FlatSubscriptionBackendImpl : public FlatSubscriptionBackend
{
public:
  FlatSubscriptionBackendImpl(const std::string & topic_name, FlatSubscriptionMode mode, FlatPayloadCallback callback)
  : topic_name_(topic_name), mode_(mode), callback_(std::move(callback))
  {
    std::string clean_topic = topic_name_.empty() ? "" : (topic_name_[0] == '/' ? topic_name_.substr(1) : topic_name_);
    iox_sub_ = std::make_unique<iox::popo::UntypedSubscriber>(iox::capro::ServiceDescription(
      iox::capro::IdString_t(iox::cxx::TruncateToCapacity, "protoros2"),
      iox::capro::IdString_t(iox::cxx::TruncateToCapacity, clean_topic),
      iox::capro::IdString_t(iox::cxx::TruncateToCapacity, "flat")));

    guard_condition_ = std::make_shared<rclcpp::GuardCondition>();
  }

  ~FlatSubscriptionBackendImpl() override { unsubscribe(); }

  void subscribe() override
  {
    if (iox_sub_ && !iox_listener_) {
      iox_sub_->subscribe();
      iox_listener_ = std::make_unique<iox::popo::Listener>();
      (void)iox_listener_->attachEvent(
        *iox_sub_, iox::popo::SubscriberEvent::DATA_RECEIVED,
        iox::popo::createNotificationCallback(on_data_received, *this));
    }
  }

  void unsubscribe() override
  {
    if (iox_listener_ && iox_sub_) {
      iox_listener_->detachEvent(*iox_sub_, iox::popo::SubscriberEvent::DATA_RECEIVED);
      iox_listener_.reset();
    }
    if (iox_sub_) {
      iox_sub_->unsubscribe();
    }
  }

  bool has_data() const override { return iox_sub_ && iox_sub_->hasData(); }

  bool take_payload(FlatPayloadCallback processor) override
  {
    if (!iox_sub_ || !iox_sub_->hasData()) {
      return false;
    }

    auto take_result = iox_sub_->take();
    if (take_result.has_error()) {
      return false;
    }

    const void * payload = take_result.value();
    auto chunk_header = iox::mepoo::ChunkHeader::fromUserPayload(payload);
    uint32_t payload_size = chunk_header ? chunk_header->userPayloadSize() : 0;

    if (payload && payload_size > 0 && processor) {
      auto vec = flatbuffers::GetRoot<flatbuffers::Vector<uint8_t>>(payload);
      if (vec && vec->data() && vec->size() > 0 && vec->size() <= payload_size) {
        processor(vec->data(), vec->size(), true);
      } else {
        processor(payload, payload_size, false);
      }
    }
    iox_sub_->release(payload);
    return true;
  }

  rclcpp::GuardCondition::SharedPtr get_guard_condition() const override { return guard_condition_; }

  iox::popo::UntypedSubscriber * get_iox_subscriber() const override { return iox_sub_.get(); }

private:
  static void on_data_received(
    iox::popo::UntypedSubscriber * const subscriber, FlatSubscriptionBackendImpl * const self) noexcept
  {
    if (!subscriber || !self) {
      return;
    }

    if (self->guard_condition_) {
      self->guard_condition_->trigger();
    }

    if (self->mode_ == FlatSubscriptionMode::CallbackPush && self->callback_) {
      while (subscriber->hasData()) {
        auto take_result = subscriber->take();
        if (!take_result.has_error()) {
          const void * payload = take_result.value();
          auto chunk_header = iox::mepoo::ChunkHeader::fromUserPayload(payload);
          uint32_t payload_size = chunk_header ? chunk_header->userPayloadSize() : 0;

          if (payload && payload_size > 0 && self->callback_) {
            auto vec = flatbuffers::GetRoot<flatbuffers::Vector<uint8_t>>(payload);
            if (vec && vec->data() && vec->size() > 0 && vec->size() <= payload_size) {
              self->callback_(vec->data(), vec->size(), true);
            } else {
              self->callback_(payload, payload_size, false);
            }
          }
          subscriber->release(payload);
        }
      }
    }
  }

  std::string topic_name_;
  FlatSubscriptionMode mode_;
  FlatPayloadCallback callback_;
  std::unique_ptr<iox::popo::UntypedSubscriber> iox_sub_;
  std::unique_ptr<iox::popo::Listener> iox_listener_;
  rclcpp::GuardCondition::SharedPtr guard_condition_;
};

std::unique_ptr<FlatPublisherBackend> create_flat_publisher_backend(const std::string & topic_name)
{
  return std::make_unique<FlatPublisherBackendImpl>(topic_name);
}

std::unique_ptr<FlatSubscriptionBackend> create_flat_subscription_backend(
  const std::string & topic_name, FlatSubscriptionMode mode, FlatPayloadCallback callback)
{
  return std::make_unique<FlatSubscriptionBackendImpl>(topic_name, mode, std::move(callback));
}

}  // namespace details
}  // namespace protoros2
