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

#include "protoros2/posh_runtime_helper.hpp"

#include <unistd.h>
#include <mutex>

#include "iceoryx_posh/runtime/posh_runtime.hpp"

namespace protoros2
{

void ensure_posh_runtime_initialized(const std::string & custom_name)
{
  static std::once_flag init_flag;
  std::call_once(init_flag, [custom_name]() {
    std::string app_name = custom_name.empty() ? ("protoros2_" + std::to_string(getpid())) : custom_name;
    try {
      iox::runtime::PoshRuntime::initRuntime(iox::capro::IdString_t(iox::cxx::TruncateToCapacity, app_name));
    } catch (...) {
      // PoshRuntime might already be initialized by host application or RouDi unreachable
    }
  });
}

}  // namespace protoros2
