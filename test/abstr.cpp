#include "fuzz.hpp"

int main() {
  ramfuzz::B::control rb(0);
  for (auto m : rb.mroulette)
    (rb.*m)();
  return (rb.obj.sum != 321);
}

unsigned ::ramfuzz::runtime::spinlimit = 3;
