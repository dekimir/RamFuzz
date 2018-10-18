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
}  // namespace

namespace ramfuzz {
namespace valgen {

void valgen(socket& sock) {
  message msg;
  sock.receive(msg);
  if (msg.parts() <= 1) {
    message errresp(22);
    sock.send(errresp);
    return;
  }
  if (is_exit_status(msg)) {
    const auto succ = is_success(msg);
    // TODO: Insert/verify tree leaf, propagate MAYWIN.
    message resp(10, is_success);
    sock.send(resp);
    return;
  } else {
    // This is a request for a value of certain type within certain bounds.
  }
}

}  // namespace valgen
}  // namespace ramfuzz
