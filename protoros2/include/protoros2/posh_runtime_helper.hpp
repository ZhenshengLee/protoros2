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

#ifndef PROTOROS2__POSH_RUNTIME_HELPER_HPP_
#define PROTOROS2__POSH_RUNTIME_HELPER_HPP_

#include <string>

namespace protoros2
{

/**
 * @brief Ensures that the Iceoryx PoshRuntime is initialized exactly once per process
 * with a unique application name derived from the process ID.
 *
 * @param custom_name Optional custom prefix or full name. If empty, defaults to "protoros2_<PID>".
 */
void ensure_posh_runtime_initialized(const std::string & custom_name = "");

}  // namespace protoros2

#endif  // PROTOROS2__POSH_RUNTIME_HELPER_HPP_
