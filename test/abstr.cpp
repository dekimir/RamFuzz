#include "fuzz.hpp"

int main() {
  ramfuzz::runtime::gen g;
  ramfuzz::B::control rb(g, 0);
  for (auto m : rb.mroulette)
    (rb.*m)();
  return (rb.obj.sum != 321);
}

unsigned ::ramfuzz::runtime::spinlimit = 3;
