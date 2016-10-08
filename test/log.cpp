#include <memory>

#include "fuzz.hpp"

bool operator!=(const ramfuzz::rfA::control &a,
                const ramfuzz::rfA::control &b) {
  if (!a && !b)
    return false;
  if (bool(a) != bool(b))
    return true;
  return a.obj != b.obj;
}

int main() {
  using namespace ramfuzz::runtime;
  using namespace std;
  unique_ptr<gen> g(new gen("fuzzlog1"));
  auto rf1 = spin_roulette<ramfuzz::rfA::control>(*g);
  g.reset(new gen("fuzzlog1", "fuzzlog2"));
  auto rf2 = spin_roulette<ramfuzz::rfA::control>(*g);
  return rf1 != rf2;
}

unsigned ::ramfuzz::runtime::spinlimit = 3;
