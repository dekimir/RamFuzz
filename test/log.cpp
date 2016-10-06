#include <memory>

#include "fuzz.hpp"

int main() {
  using namespace ramfuzz::runtime;
  using namespace std;
  unique_ptr<gen> g(new gen("fuzzlog1"));
  auto rf1 = spin_roulette<ramfuzz::rfA::control>(*g);
  g.reset(new gen("fuzzlog1", "fuzzlog2"));
  auto rf2 = spin_roulette<ramfuzz::rfA::control>(*g);
  return rf1.obj != rf2.obj;
}

unsigned ::ramfuzz::runtime::spinlimit = 3;
