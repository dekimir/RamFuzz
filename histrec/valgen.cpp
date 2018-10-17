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
bool is_exit_status(const message& msg) { return false; }
}  // namespace

namespace ramfuzz {
namespace histrec {

void valgen(socket& sock) {
  message msg;
  sock.receive(msg);
  // Either exit status or a request for a value.
  if (const auto status = is_exit_status(msg)) {
    // Insert/verify tree leaf, propagate MAYWIN.
  } else {
    // This is a request for a value of certain type within certain bounds.
  }
  // generate random value
  // send it back
}

}  // namespace histrec
}  // namespace ramfuzz
