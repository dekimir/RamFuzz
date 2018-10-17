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

using namespace ramfuzz::histrec;
using namespace std;
using namespace zmqpp;

namespace {

class ValgenTest : public ::testing::Test {
 protected:
  context ctx;
  socket to_histrec, from_ramfuzz;

 public:
  ValgenTest()
      : to_histrec(ctx, socket_type::request),
        from_ramfuzz(ctx, socket_type::reply) {
    to_histrec.set(socket_option::linger, 0);
    from_ramfuzz.set(socket_option::linger, 0);
    from_ramfuzz.bind("ipc://*");  // TODO: can't use ipc on Windows.
    to_histrec.connect(from_ramfuzz.get<string>(socket_option::last_endpoint));
  }
};

constexpr bool DONT_BLOCK = true;

TEST_F(ValgenTest, MessageTooShort) {
  message msg(true);
  ASSERT_TRUE(to_histrec.send(msg, DONT_BLOCK));
  valgen(from_ramfuzz);
  ASSERT_TRUE(to_histrec.receive(msg));
  ASSERT_EQ(1, msg.parts());
  EXPECT_EQ(22, msg.get<int>(0));
}

}  // namespace
