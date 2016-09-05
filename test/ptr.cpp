#include "fuzz.hpp"

int main() {
  B b;
  ramfuzz::B::control rf(b);
  for (auto m : rf.mroulette)
    (rf.*m)();
  if (b.sum != 3)
    return 1;
}

unsigned ::ramfuzz::runtime::spinlimit = 3;
