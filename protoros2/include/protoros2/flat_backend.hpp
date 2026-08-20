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

#ifndef PROTOROS2__FLAT_BACKEND_HPP_
#define PROTOROS2__FLAT_BACKEND_HPP_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "google/protobuf/message.h"
#include "rclcpp/guard_condition.hpp"

namespace protoros2
{

/// Default upper bound for a single flat-channel payload (iceoryx2 slice size).
inline constexpr size_t kDefaultMaxFlatPayloadBytes = 10 * 1024 * 1024;

enum class FlatSubscriptionMode
{
  CallbackPush = 0,
  WaitSetPull = 1
};

namespace details
{

class FlatPublisherBackend
{
public:
  virtual ~FlatPublisherBackend() = default;
  virtual void publish_protobuf(const google::protobuf::Message & proto_msg) = 0;
  virtual void publish_raw(const void * data, size_t size) = 0;
  virtual void * get_backend_handle() const = 0;
  /// Number of publishes dropped due to loan/serialize/send failures.
  virtual size_t publish_fail_count() const = 0;
  /// True when all underlying middleware entities were created successfully.
  virtual bool valid() const = 0;
};

using FlatPayloadCallback = std::function<void(const void * payload, size_t size)>;

class FlatSubscriptionBackend
{
public:
  virtual ~FlatSubscriptionBackend() = default;
  virtual void subscribe() = 0;
  virtual bool has_data() const = 0;
  virtual bool take_payload(FlatPayloadCallback processor) = 0;
  virtual rclcpp::GuardCondition::SharedPtr get_guard_condition() const = 0;
  virtual void * get_backend_handle() const = 0;
  /// True when all underlying middleware entities were created successfully.
  virtual bool valid() const = 0;
};

class EnterpriseNodeBackend
{
public:
  virtual ~EnterpriseNodeBackend() = default;
  /// True when the underlying middleware node and waitset were created successfully.
  virtual bool valid() const = 0;
};

std::shared_ptr<EnterpriseNodeBackend> create_enterprise_node_backend(const std::string & node_name);

std::unique_ptr<FlatPublisherBackend> create_flat_publisher_backend(
  std::shared_ptr<EnterpriseNodeBackend> node_backend, const std::string & topic_name,
  size_t max_payload_bytes = kDefaultMaxFlatPayloadBytes);

std::shared_ptr<FlatSubscriptionBackend> create_flat_subscription_backend(
  std::shared_ptr<EnterpriseNodeBackend> node_backend, const std::string & topic_name, FlatSubscriptionMode mode,
  FlatPayloadCallback callback);

}  // namespace details
}  // namespace protoros2

#endif  // PROTOROS2__FLAT_BACKEND_HPP_
