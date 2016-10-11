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

constexpr int mspins = 10; ///< How many times to spin rfA's roulette.

vector<int> spin(gen &g) {
  A a;
  ramfuzz::rfA::control rf(g, a);
  for (int i = 0; i < mspins; ++i)
    rf.f0();
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

/// For each (zero-based) subscript in subs, finds fuzzlog1 start/end position
/// of that value.  If, eg, subs={3, 5, 9}, returns the start/end positions for
/// 3rd, 5th, and 9th value in fuzzlog1.
positions value_positions(const vector<size_t> &subs) {
  positions pos;
  if (subs.empty())
    return pos;
  auto next_sub = subs.cbegin();
  ifstream idx("fuzzlog1.i");
  for (int i = 0; i < mspins; ++i) {
    char c;
    if (!idx.get(c) || c != '0')
      exit(2);
    if (!idx.get(c) || c != '|')
      exit(3);
    streamoff start, end;
    if (!(idx >> start))
      exit(4);
    if (!(idx >> end))
      exit(5);
    if (i == *next_sub) {
      pos.push_back(make_pair(start, end));
      if (++next_sub == subs.cend())
        return pos;
    }
    idx.ignore(999999, '\n');
  }
  return pos;
}

/// Creates a random set of valid value subscripts using RamFuzz runtime.  Logs
/// values in log.
vector<size_t> rnd_subs(const string &log) {
  gen g(log);
  vector<size_t> subs;
  for (int i = 0; i < g.between(0, mspins); ++i)
    subs.push_back(i);
  return subs;
}

int main() {
  const auto r1 = first_run();
  const auto to_skip = rnd_subs("to-skip");
  {
    ofstream ctl("fuzzlog1.c");
    for (auto p : value_positions(to_skip))
      ctl << p.first << ' ' << p.second << endl;
  }
  const auto r2 = second_run();
  auto s = to_skip.cbegin();
  for (int i = 0; i < mspins; ++i) {
    if (s != to_skip.cend() && i == *s) {
      ++s;
      continue;
    }
    if (r1[i] != r2[i])
      return 1;
  }
}

unsigned ::ramfuzz::runtime::spinlimit = 3;
