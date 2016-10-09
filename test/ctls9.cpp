#include <fstream>
#include <utility>

#include "fuzz.hpp"

using namespace ramfuzz::runtime;
using namespace std;

constexpr int mspins = 10; ///< How many times to spin rfA's roulette.

/// Spins rfA's roulette, logging generated values in file fuzzlog1.  Returns
/// the accumulated parameter values.
vector<int> first_run() {
  A a;
  gen g("fuzzlog1");
  ramfuzz::rfA::control rf(g, a);
  for (int i = 0; i < mspins; ++i)
    rf.f0();
  return a.vi;
}

/// Replays first run, obeying fuzzlog1.c.  Returns the accumulated parameter
/// values.
vector<int> second_run() {
  A a;
  gen g("fuzzlog1", "fuzzlog2");
  ramfuzz::rfA::control rf(g, a);
  for (int i = 0; i < mspins; ++i)
    rf.f0();
  return a.vi;
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

int main() {
  const vector<size_t> to_skip = {9};
  const auto r1 = first_run();
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
