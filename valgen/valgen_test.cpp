// Copyright 2016-2018 The RamFuzz contributors. All rights reserved.
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
#include <unistd.h>
#include <string>
#include <zmqpp/context.hpp>
#include <zmqpp/message.hpp>

#include "valgen.hpp"

using namespace ramfuzz::valgen;
using namespace std;
using namespace zmqpp;

namespace {

class ValgenTest : public ::testing::Test {
 protected:
  context ctx;
  socket to_valgen, from_ramfuzz;

 public:
  ValgenTest()
      : to_valgen(ctx, socket_type::request),
        from_ramfuzz(ctx, socket_type::reply) {
    to_valgen.set(socket_option::linger, 0);
    from_ramfuzz.set(socket_option::linger, 0);
    from_ramfuzz.bind("ipc://*");  // TODO: can't use ipc on Windows.
    to_valgen.connect(from_ramfuzz.get<string>(socket_option::last_endpoint));
  }

  /// Sends msg to valgen to process and records valgen's response.
  ///
  /// Returns void so it can use ASSERT_* macros.
  void valgen_roundtrip(message& msg, message& resp) {
    ASSERT_TRUE(to_valgen.send(msg));
    valgen(from_ramfuzz);
    ASSERT_TRUE(to_valgen.receive(resp));
  }
};

constexpr bool IS_EXIT = true, IS_SUCCESS = true;

// TODO: Make msg const below, when zmqpp allows it.

/// Recursion termination for the general template below.
template <int part = 0>
int part_mismatch(message& msg) {
  if (part + 1 < msg.parts()) return part + 1;  // Too many actuals.
  return -1;
}

/// Finds a mismatch between msg and its expected parts (the remaining args)
///
/// Matching starts at msg[part], which must equal nextpart, then proceeds down
/// the rest of args and msg parts.
///
/// If there is a mismatch, returns the 0-based index of msg part that doesn't
/// match.  Otherwise, returns -1.
template <int part = 0, typename T, typename... Args>
int part_mismatch(message& msg, const T nextpart, const Args... args) {
  if (part >= msg.parts() || nextpart != msg.get<T>(part)) return part;
  return part_mismatch<part + 1>(msg, args...);
}

/// Convenience shortcut for testing messages: EXPECT_PARTS(msg, part0, part1,
/// part2) will pass iff msg has exactly the specified parts.  Otherwise, it
/// will fail and print (after "Which is:") the index of msg part that
/// mismatches the expected list.
///
/// Part types are inferred: EXPECT_PARTS(msg, 0, true, -12L, 123U) expects an
/// int, a bool, a long, and an unsigned.
#define EXPECT_PARTS(...) EXPECT_EQ(-1, part_mismatch(__VA_ARGS__))

TEST_F(ValgenTest, MessageTooShort) {
  message msg(IS_EXIT), resp;
  valgen_roundtrip(msg, resp);
  EXPECT_PARTS(resp, 22);
}

TEST_F(ValgenTest, ExitSuccess) {
  message msg(IS_EXIT, IS_SUCCESS), resp;
  valgen_roundtrip(msg, resp);
  EXPECT_PARTS(resp, 10, IS_SUCCESS);
}

TEST_F(ValgenTest, ExitFailure) {
  message msg(IS_EXIT, !IS_SUCCESS), resp;
  valgen_roundtrip(msg, resp);
  EXPECT_PARTS(resp, 10, !IS_SUCCESS);
}

TEST_F(ValgenTest, RequestInt) {
  message msg(!IS_EXIT, uint64_t{123}, uint8_t{5}, 33, 55), resp;
  valgen_roundtrip(msg, resp);
  EXPECT_PARTS(resp, 11, 22);
}

}  // namespace
