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

#include <cstdlib>
#include <zmqpp/context.hpp>
#include <zmqpp/socket.hpp>

#include "valgen.hpp"

using namespace ramfuzz;
using namespace zmqpp;

int main(int argc, char* argv[]) {
  context ctx;
  // socket type?
  //  - req: simple, still able to connect multiple peers
  //    [http://zguide.zeromq.org/page:all#Recap-of-Request-Reply-Sockets], but
  //    identity hidden
  //  - router:
  //    [http://zguide.zeromq.org/page:all#ROUTER-Broker-and-REQ-Workers] shows
  //    identity, allows multi-threaded generation, FWIW
  socket s(ctx, socket_type::reply);
  s.bind(argc > 1 ? argv[1] : "ipc:///tmp/ramfuzz-socket");
  valgen vg(argc > 2 ? atoi(argv[2]) : 0);
  for (;;) vg.process_request(s);
}