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

constexpr int regcnt = 5;     ///< How many regions to generate.
constexpr int val_in_reg = 3; ///< How many values per region.

/// Spins rfA's roulette, logging generated values in file fuzzlog1.  Returns
/// the accumulated parameter values.
vector<int> first_run() {
  A a;
  gen g("fuzzlog1");
  ramfuzz::rfA::control rf(g, a);
  for (int reg = 0; reg < regcnt; ++reg) {
    gen::region reg_raii(g);
    for (int val = 0; val < val_in_reg; ++val)
      rf.f0();
  }
  return a.vi;
}

/// Replays first run, obeying fuzzlog1.c.  Returns the accumulated parameter
/// values.
vector<int> second_run() {
  A a;
  gen g("fuzzlog1", "fuzzlog2");
  ramfuzz::rfA::control rf(g, a);
  for (int reg = 0; reg < regcnt; ++reg) {
    gen::region reg_raii(g);
    for (int val = 0; val < val_in_reg; ++val)
      rf.f0();
  }
  return a.vi;
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

/// Creates a random set of valid region ids using RamFuzz runtime.  Logs values
/// in log.
vector<uint64_t> rnd_ids(const string &log) {
  gen g(log);
  vector<uint64_t> subs;
  for (int i = 1; i <= g.between(0, regcnt); ++i)
    subs.push_back(i);
  return subs;
}

int main() {
  const vector<uint64_t> to_skip = rnd_ids("to-skip");
  const auto r1 = first_run();
  {
    ofstream ctl("fuzzlog1.c");
    for (auto p : region_positions(to_skip))
      ctl << p.first << ' ' << p.second << endl;
  }
  const auto r2 = second_run();
  auto skip = to_skip.cbegin();
  size_t i = 0;
  for (int reg = 1; reg <= regcnt; ++reg) {
    for (int val = 0; val < val_in_reg; ++val) {
      if ((skip == to_skip.cend() || reg != *skip) && r1[i] != r2[i])
        exit(1);
      ++i;
    }
    if (skip != to_skip.cend() && reg == *skip)
      ++skip;
  }
}

unsigned ::ramfuzz::runtime::spinlimit = 3;
