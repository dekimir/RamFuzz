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

// TODO: make msg const in all is_* functions below, when zmqpp allows it.
bool is_exit_status(message& msg) { return msg.get<bool>(0); }

bool is_success(message& msg) { return msg.get<bool>(1); }

template <typename T>
void add_typed_value(message& req, message& resp) {
  T lo = req.get<T>(3);
  T hi = req.get<T>(4);
  resp << hi - lo;
}

/// Specializes add_typed_value for type1 to use type2 instead.
#define SPEC(type1, type2)                                     \
  template <>                                                  \
  void add_typed_value<type1>(message & req, message & resp) { \
    add_typed_value<type2>(req, resp);                         \
  }

SPEC(char, int);
SPEC(long, int64_t);
SPEC(unsigned long, uint64_t);
#undef SPEC

void add_value(message& req, message& resp) {
  switch (req.get<uint8_t>(2)) {
    // The following must match the specializations of
    // ramfuzz::runtime::typetag.
    case 0:
      return add_typed_value<bool>(req, resp);
    case 1:
      return add_typed_value<char>(req, resp);
    case 2:
      return add_typed_value<unsigned char>(req, resp);
    case 3:
      return add_typed_value<short>(req, resp);
    case 4:
      return add_typed_value<unsigned short>(req, resp);
    case 5:
      return add_typed_value<int>(req, resp);
    case 6:
      return add_typed_value<unsigned int>(req, resp);
    case 7:
      return add_typed_value<long>(req, resp);
    case 8:
      return add_typed_value<unsigned long>(req, resp);
    case 9:
      return add_typed_value<long long>(req, resp);
    case 10:
      return add_typed_value<unsigned long long>(req, resp);
    case 11:
      return add_typed_value<float>(req, resp);
    case 12:
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
    return response(sock, 22);
  else if (is_exit_status(msg)) {
    if (msg.parts() != 2) return response(sock, 23);
    const auto succ = is_success(msg);
    // TODO: Insert/verify tree leaf, propagate MAYWIN.
    return response(sock, 10, succ);
  } else {
    // This is a request for a value of certain type within certain bounds.
    // Message is (false, uint64_t value_id, uint8_t tag, T lo, T hi), where T
    // is identified by tag.
    if (msg.parts() != 5) return response(sock, 24);
    message resp(11);
    add_value(msg, resp);
    sock.send(resp);
  }
}

}  // namespace valgen
}  // namespace ramfuzz
