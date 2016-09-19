#include "fuzz.hpp"

int main() {
  ramfuzz::runtime::gen g;
  B b;
  ramfuzz::qqB::control rf(g, b);
  for (auto m : rf.mroulette)
    (rf.*m)();
  if (b.sum != 3)
    return 1;
}

unsigned ::ramfuzz::runtime::spinlimit = 3;
