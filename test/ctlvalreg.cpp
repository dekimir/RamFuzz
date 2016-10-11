// Copyright 2016 The RamFuzz contributors. All rights reserved.
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

#include <fstream>
#include <utility>

#include "fuzz.hpp"

using namespace ramfuzz::runtime;
using namespace std;

vector<int> run(gen &g) {
  A a;
  ramfuzz::rfA::control rf(g, a);
  {
    gen::region r1(g);
    rf.f0(); // vi[0]
  }
  rf.f0(); // vi[1]
  {
    gen::region r2(g);
    rf.f0(); // vi[2]
  }
  rf.f0(); // vi[3]
  return a.vi;
}

/// Spins rfA's roulette, logging generated values in file fuzzlog1.  Returns
/// the accumulated parameter values.
vector<int> first_run() {
  gen g("fuzzlog1");
  return run(g);
}

/// Replays first run, obeying fuzzlog1.c.  Returns the accumulated parameter
/// values.
vector<int> second_run() {
  gen g("fuzzlog1", "fuzzlog2");
  return run(g);
}

int main() {
  const vector<uint64_t> to_skip = {3};
  const auto r1 = first_run();
  {
    ofstream ctl("fuzzlog1.c");
    ifstream idx("fuzzlog1.i");
    idx.ignore(999999, '\n'); // r1 start
    idx.ignore(999999, '\n'); // r1 value
    idx.ignore(999999, '\n'); // r1 end
    // Loose value between r1 and r2:
    idx.ignore(1, '0');
    idx.ignore(1, '|');
    streamoff pos;
    if (!(idx >> pos))
      exit(11);
    ctl << pos << ' ';
    if (!(idx >> pos))
      exit(13);
    ctl << pos << endl;
    idx.ignore(999999, '\n');
    uint64_t id;
    if (!(idx >> id) || id != 2)
      exit(15);
    idx.ignore(1, '{'); // r2 start
    if (!(idx >> pos))
      exit(17);
    ctl << pos << ' ';
    idx.ignore(999999, '\n');
    idx.ignore(999999, '\n'); // r2 value
    if (!(idx >> id) || id != 2)
      exit(19);
    idx.ignore(1, '}'); // r2 end
    if (!(idx >> pos))
      exit(21);
    ctl << pos << endl;
  }
  const auto r2 = second_run();
  return r1[0] != r2[0] || r1[3] != r2[3];
}

unsigned ::ramfuzz::runtime::spinlimit = 3;
