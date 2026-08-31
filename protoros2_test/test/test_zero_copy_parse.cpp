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

#include <gtest/gtest.h>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>

#include "google/protobuf/message.h"
#include "google/protobuf/stubs/stringpiece.h"
#include "protoros2_test/msg/bench_bytes__typeadapter_protobuf_cpp.hpp"

namespace
{

constexpr size_t kPayloadSize = 3 * 1024 * 1024;

std::string MakeSerializedPayload()
{
  protoros2_test::msg::pb::BenchBytes msg;
  std::string filler(kPayloadSize, '\0');
  for (size_t i = 0; i < filler.size(); ++i) {
    filler[i] = static_cast<char>((i * 31u + 7u) & 0xFFu);
  }
  msg.set_data(filler);
  std::string out;
  EXPECT_TRUE(msg.SerializeToString(&out));
  return out;
}

}  // namespace

// Verifies ParseFrom<kParseWithAliasing> parses correctly and reports whether the
// bytes field becomes a direct view into the input buffer. NOTE: aliasing behavior is
// protobuf-version dependent - 3.21.12 does NOT actually alias bytes fields despite the
// flag; true zero-copy parsing requires CORD/string_view (protobuf >= 22/26). The check
// is therefore informational, not a hard assertion.
TEST(ZeroCopyParse, AliasingParsesCorrectlyAndViewsInputBuffer)
{
  const std::string buffer = MakeSerializedPayload();

  protoros2_test::msg::pb::BenchBytes aliased;
  ASSERT_TRUE(aliased.ParseFrom<google::protobuf::MessageLite::kParseWithAliasing>(
    google::protobuf::StringPiece(buffer.data(), buffer.size())));
  ASSERT_EQ(aliased.data().size(), kPayloadSize);

  EXPECT_EQ(static_cast<uint8_t>(aliased.data()[0]), 7u);
  const size_t last = kPayloadSize - 1;
  EXPECT_EQ(static_cast<uint8_t>(aliased.data()[last]), static_cast<uint8_t>((last * 31u + 7u) & 0xFFu));

  const char * buf_begin = buffer.data();
  const char * buf_end = buffer.data() + buffer.size();
  const char * field_begin = aliased.data().data();
  const bool aliased_in_buffer = (field_begin >= buf_begin) && (field_begin + kPayloadSize <= buf_end);
  std::cout << "[ZeroCopyParse] bytes field aliased into input buffer: " << (aliased_in_buffer ? "YES" : "NO")
            << " (version-dependent; NO on protobuf 3.21.12)" << std::endl;
}

TEST(ZeroCopyParse, CopyVsAliasingTiming)
{
  const std::string buffer = MakeSerializedPayload();
  constexpr int kIterations = 50;

  auto time_parse = [&](bool alias) {
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < kIterations; ++i) {
      protoros2_test::msg::pb::BenchBytes msg;
      bool ok = false;
      if (alias) {
        ok = msg.ParseFrom<google::protobuf::MessageLite::kParseWithAliasing>(
          google::protobuf::StringPiece(buffer.data(), buffer.size()));
      } else {
        ok = msg.ParseFromArray(buffer.data(), static_cast<int>(buffer.size()));
      }
      EXPECT_TRUE(ok);
      EXPECT_EQ(static_cast<uint8_t>(msg.data()[0]), 7u);
    }
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count() / kIterations;
  };

  const double copy_ms = time_parse(false);
  const double alias_ms = time_parse(true);
  std::cout << "[ZeroCopyParse] 3MB parse avg over " << kIterations << " iterations: copy=" << copy_ms
            << " ms, aliasing=" << alias_ms << " ms, saved=" << copy_ms - alias_ms << " ms ("
            << (copy_ms > 0.0 ? (copy_ms - alias_ms) / copy_ms * 100.0 : 0.0) << "%)" << std::endl;
}
