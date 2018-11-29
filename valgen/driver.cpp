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

/// Drives interaction between a test using RamFuzz runtime and a valgen
/// instance.  The runtime communicates with valgen to obtain random values it
/// feeds to the test code.  The test uses these values to exercise code under
/// test, resulting in either success or failure.  Valgen, for its part, wants
/// to know the test outcome so it can learn how to generate valid random
/// values.  But this requires two things that the test executable cannot or
/// shouldn't provide:
///
/// - the test should be run many times against the same valgen instance,
///   providing valgen with sufficient training data;
///
/// - if the test execution aborts before completion for any reason (eg, a
///   segfault or an unhandled exception), someone needs to signal failure to
///   valgen
///
/// This driver program provides the above two functions.  It runs the test
/// repeatedly, and when each run is finished (or aborted), it sends valgen one
/// final message to signal success or failure, depending on the test's exit
/// status.
///
/// The driver provides the valgen endpoint (ie, the ZMQ address on which valgen
/// listens for messages) as an argument to the test executable.  It uses the
/// same endpoint for its own success/failure messages it sends to valgen.

#include <unistd.h>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <string>
#include <zmqpp/context.hpp>
#include <zmqpp/message.hpp>
#include <zmqpp/socket.hpp>

#include "../runtime/ramfuzz-rt.hpp"
#include "status.hpp"

using namespace ramfuzz;
using namespace std;
using namespace zmqpp;

int main(int argc, const char** argv) {
  static constexpr size_t default_count = 1000;
  if (argc < 2) {
    cerr << "usage: " << argv[0] << " <test executable> [<count> [<endpoint>]]"
         << endl;
    cerr << "defaults: count=" << default_count
         << ", endpoint=" << runtime::default_valgen_endpoint << endl;
    exit(11);
  }
  const auto count = (argc < 3) ? default_count : atoi(argv[2]);
  const auto endpoint = (argc < 4) ? runtime::default_valgen_endpoint : argv[3];
  const string command = string(argv[1]) + ' ' + endpoint;
  context ctx;
  const char line_reset = isatty(STDOUT_FILENO) ? '\r' : '\n';
  for (int i = 1; i <= count; ++i) {
    const uint8_t success = (0 == system(command.c_str()));
    message m(uint8_t{1}, success);
    socket sock(ctx, socket_type::request);
    sock.connect(endpoint);
    sock.send(m);
    const auto received = sock.receive(m);
    if (!received) {
      cerr << "Failed to receive from valgen's socket." << endl;
      exit(22);
    }
    const auto status = m.get<uint8_t>(0);
    if (status != ResponseStatus::OK_TERMINAL) {
      cerr << "Received unexpected status in valgen's response: " << status
           << endl;
      exit(33);
    }
    assert(m.get<uint8_t>(1) == success);
    cout << i << line_reset << flush;
  }
  cout << endl;
}
