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

#include <cassert>
#include <zmqpp/message.hpp>

using namespace std;
using namespace zmqpp;
using namespace ramfuzz::exetree;

namespace {

using u8 = uint8_t;

u8 is_exit_status(const message& msg) { return msg.get<u8>(0); }

u8 is_success(const message& msg) { return msg.get<u8>(1); }

template <typename T>
T uniform_random(T lo, T hi, ranlux24& rn_eng) {
  return uniform_int_distribution<T>{lo, hi}(rn_eng);
}

template <>
double uniform_random(double lo, double hi, ranlux24& rn_eng) {
  return uniform_real_distribution<double>{lo, hi}(rn_eng);
}

template <typename T>
void add_typed_value(message& req, message& resp, ranlux24& rn_eng,
                     node*& cursor) {
  T lo = req.get<T>(3);
  T hi = req.get<T>(4);
  T v = uniform_random(lo, hi, rn_eng);
  cursor = cursor->find_or_add_edge(v);
  resp << v;
}

void add_value(message& req, message& resp, ranlux24& rn_eng, node*& cursor) {
  switch (req.get<uint8_t>(2)) {
    // The following must match the specializations of
    // ramfuzz::runtime::typetag.
    case 1:
      return add_typed_value<int64_t>(req, resp, rn_eng, cursor);
    case 2:
      return add_typed_value<uint64_t>(req, resp, rn_eng, cursor);
    case 3:
      return add_typed_value<double>(req, resp, rn_eng, cursor);
    default:
      assert(false);
  }
}

template <typename... Args>
void response(socket& sock, Args&&... args) {
  message resp(forward<Args>(args)...);
  sock.send(resp);
}

}  // namespace

namespace ramfuzz {

void valgen::process_request(socket& sock) {
  message msg;
  sock.receive(msg);
  if (msg.parts() <= 1)
    return response(sock, ResponseStatus::ERR_FEW_PARTS);
  else if (is_exit_status(msg)) {
    if (msg.parts() != 2)
      return response(sock, ResponseStatus::ERR_TERM_TAKES_2);
    // TODO: check current cursor->terminal value!
    const auto succ = is_success(msg);
    cursor->terminal(succ ? node::SUCCESS : node::FAILURE);
    cursor->maywin(succ);
    if (succ) {
      while (auto up = cursor->incoming_edge()) {
        cursor = up->src();
        cursor->maywin(true);
      }
      assert(cursor == &root);
    }
    cursor = &root;
    return response(sock, ResponseStatus::OK_TERMINAL, succ);
  } else {
    // This is a request for a value of certain type within certain bounds.
    // Message is (uint8_t type, uint64_t value_id, uint8_t tag, T lo, T hi),
    // where T is identified by tag (see add_typed_value() above).
    if (msg.parts() != 5)
      return response(sock, ResponseStatus::ERR_VALUE_TAKES_5);
    uint64_t valueid;
    msg.get(valueid, 1);
    if (!cursor->check_valueid(valueid))
      return response(sock, ResponseStatus::ERR_WRONG_VALUEID);
    cursor->set_valueid(valueid);
    message resp(u8{11});
    add_value(msg, resp, rn_eng, cursor);
    sock.send(resp);
  }
}

const uint8_t valgen::ResponseStatus::OK_TERMINAL,
    valgen::ResponseStatus::OK_VALUE, valgen::ResponseStatus::ERR_FEW_PARTS,
    valgen::ResponseStatus::ERR_TERM_TAKES_2,
    valgen::ResponseStatus::ERR_VALUE_TAKES_5,
    valgen::ResponseStatus::ERR_WRONG_VALUEID,
    valgen::ResponseStatus::END_MARKER_DO_NOT_USE;

}  // namespace ramfuzz
