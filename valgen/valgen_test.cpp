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
#include <limits>
#include <string>
#include <typeinfo>
#include <zmqpp/context.hpp>
#include <zmqpp/message.hpp>

#include "valgen.hpp"

using namespace ramfuzz;
using namespace std;
using namespace zmqpp;

extern unique_ptr<valgen> global_valgen;
extern unique_ptr<mt19937> global_testrng;

namespace {

using u8 = uint8_t;
using i64 = int64_t;
using u64 = uint64_t;

constexpr u8 IS_EXIT = 1, IS_SUCCESS = 1;

template <typename T>
struct widetype;

#define WIDETYPE(T, W) \
  template <>          \
  struct widetype<T> { \
    using type = W;    \
  }

WIDETYPE(bool, i64);
WIDETYPE(char, i64);
WIDETYPE(short, i64);
WIDETYPE(int, i64);
WIDETYPE(long, i64);
WIDETYPE(long long, i64);

#undef WIDETYPE

template <typename T>
u8 typetag();

#define TYPETAG(T, tag) \
  template <>           \
  u8 typetag<T>() {     \
    return tag;         \
  }

TYPETAG(i64, 1);
TYPETAG(u64, 2);
TYPETAG(double, 3);

#undef TYPETAG

template <typename T>
T random(T lo = numeric_limits<T>::min(), T hi = numeric_limits<T>::max()) {
  uniform_int_distribution<T> inst(lo, hi);
  return inst(*global_testrng);
}

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

  /// Sends msg to valgen to process, receives valgen's response, and returns
  /// it.
  message valgen_roundtrip(message& msg) {
    EXPECT_TRUE(to_valgen.send(msg));
    global_valgen->process_request(from_ramfuzz);
    message resp;
    EXPECT_TRUE(to_valgen.receive(resp));
    return resp;
  }

  /// Uses valgen to generate a random value between lo and hi, then checks that
  /// the value is indeed between these bounds.
  template <typename T>
  void valgen_between(T lo, T hi) {
    using W = typename widetype<T>::type;
    message msg(!IS_EXIT, u64{123}, typetag<W>(), W{lo}, W{hi});
    const auto resp = valgen_roundtrip(msg);
    const auto tname = typeid(lo).name();
    ASSERT_EQ(11, resp.get<u8>(0))
        << "type: " << tname << ", lo: " << lo << ", hi: " << hi;
    const auto val = static_cast<T>(resp.get<W>(1));
    EXPECT_LE(lo, val) << tname;
    EXPECT_GE(hi, val) << tname;
  }

  /// Generates random bounds of type T and invokes valgen_between() on them.
  template <typename T>
  void check_random_bounds() {
    static const auto max = numeric_limits<T>::max();
    const auto lo = random<T>(), range = random<T>(0);
    const T hi = (range < max - lo) ? lo + range : max;
    valgen_between(lo, hi);
  }
};

/// Recursion termination for the general template below.
template <int part = 0>
int part_mismatch(const message& msg) {
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
int part_mismatch(const message& msg, const T nextpart, const Args... args) {
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
  message msg(IS_EXIT);
  EXPECT_PARTS(valgen_roundtrip(msg), u8{22});
}

TEST_F(ValgenTest, ExitSuccess) {
  message msg(IS_EXIT, IS_SUCCESS);
  EXPECT_PARTS(valgen_roundtrip(msg), u8{10}, IS_SUCCESS);
}

TEST_F(ValgenTest, ExitFailure) {
  message msg(IS_EXIT, !IS_SUCCESS);
  EXPECT_PARTS(valgen_roundtrip(msg), u8{10}, !IS_SUCCESS);
}

TEST_F(ValgenTest, BetweenBool) { check_random_bounds<bool>(); }
TEST_F(ValgenTest, BetweenChar) { check_random_bounds<char>(); }
TEST_F(ValgenTest, BetweenShort) { check_random_bounds<short>(); }
TEST_F(ValgenTest, BetweenInt) { check_random_bounds<int>(); }
TEST_F(ValgenTest, BetweenLong) { check_random_bounds<long>(); }
TEST_F(ValgenTest, BetweenLongLong) { check_random_bounds<long long>(); }

}  // namespace
