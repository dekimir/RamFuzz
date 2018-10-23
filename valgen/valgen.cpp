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

#include "valgen.hpp"

#include <zmqpp/message.hpp>

using namespace zmqpp;

namespace {

using u8 = uint8_t;

u8 is_exit_status(const message& msg) { return msg.get<u8>(0); }

u8 is_success(const message& msg) { return msg.get<u8>(1); }

template <typename T>
void add_typed_value(message& req, message& resp) {
  T lo = req.get<T>(3);
  T hi = req.get<T>(4);
  resp << hi - lo;
}

void add_value(message& req, message& resp) {
  switch (req.get<uint8_t>(2)) {
    // The following must match the specializations of
    // ramfuzz::runtime::typetag.
    case 1:
      return add_typed_value<int64_t>(req, resp);
    case 2:
      return add_typed_value<uint64_t>(req, resp);
    case 3:
      return add_typed_value<double>(req, resp);
    default:
      assert(false);
  }
}

template <typename... Args>
void response(socket& sock, Args&&... args) {
  message resp(std::forward<Args>(args)...);
  sock.send(resp);
}

}  // namespace

namespace ramfuzz {
namespace valgen {

void valgen(socket& sock) {
  message msg;
  sock.receive(msg);
  if (msg.parts() <= 1)
    return response(sock, u8{22});
  else if (is_exit_status(msg)) {
    if (msg.parts() != 2) return response(sock, 23);
    const auto succ = is_success(msg);
    // TODO: Insert/verify tree leaf, propagate MAYWIN.
    return response(sock, u8{10}, succ);
  } else {
    // This is a request for a value of certain type within certain bounds.
    // Message is (false, uint64_t value_id, uint8_t tag, T lo, T hi), where T
    // is identified by tag (see add_value() above).
    if (msg.parts() != 5) return response(sock, u8{24});
    message resp(u8{11});
    add_value(msg, resp);
    sock.send(resp);
  }
}

}  // namespace valgen
}  // namespace ramfuzz
