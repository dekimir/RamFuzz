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

vector<int> spin(gen &g) {
  A a;
  ramfuzz::harness<A> rf(g, a);
  {
    gen::region r1(g);
    {
      gen::region r2(g);
      rf.f0(); // vi[0]
    }
    rf.f0(); // vi[1]
  }
  gen::region r3(g);
  rf.f0(); // vi[2]
  {
    gen::region r4(g);
    rf.f0(); // vi[3]
  }
  rf.f0(); // vi[4]
  return a.vi;
}

/// Spins rfA's roulette, logging generated values in file fuzzlog1.  Returns
/// the accumulated parameter values.
vector<int> first_run() {
  gen g("fuzzlog1");
  return spin(g);
}

/// Replays first run, obeying fuzzlog1.c.  Returns the accumulated parameter
/// values.
vector<int> second_run() {
  gen g("fuzzlog1", "fuzzlog2");
  return spin(g);
}

using positions = vector<pair<streamoff, streamoff>>;

/// For each region id, finds fuzzlog1 start/end position of that region.
positions region_positions(const vector<uint64_t> &ids) {
  const auto n = ids.size();
  positions pos(n);
  if (ids.empty())
    return pos;
  vector<unsigned> lines_matching(n);
  ifstream idx("fuzzlog1.i");
  uint64_t regid;
  while (idx >> regid) {
    size_t reg = 0;
    while (reg < n && ids[reg] != regid)
      ++reg;
    if (reg < n) {
      lines_matching[reg]++;
      char c;
      if (!idx.get(c))
        exit(12); // ID not followed by {, }, or |.
      streamoff off;
      if (!(idx >> off))
        exit(14); // Position missing.
      switch (c) {
      case '{':
        pos[reg].first = off;
        break;
      case '}':
        pos[reg].second = off;
        break;
      default:
        exit(16); // Not a region: either ids has zeros or fuzzlog1.i is bad.
      }
    }
    idx.ignore(999999, '\n');
  }
  if (lines_matching != vector<unsigned>(n, 2u))
    exit(18); // Some region matches more (or fewer) than 2 lines.
  return pos;
}

int main() {
  const vector<uint64_t> to_skip = {2, 3};
  const auto r1 = first_run();
  {
    ofstream ctl("fuzzlog1.c");
    for (auto p : region_positions(to_skip))
      ctl << p.first << ' ' << p.second << endl;
  }
  const auto r2 = second_run();
  return r1[1] != r2[1];
}

unsigned ::ramfuzz::runtime::spinlimit = 3;
