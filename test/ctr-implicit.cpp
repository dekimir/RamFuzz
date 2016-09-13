#include "fuzz.hpp"

int main() {
  ramfuzz::runtime::gen g;
  ramfuzz::B::control rb(g, ramfuzz::B::control::ccount - 1);
  for (auto m : rb.mroulette)
    (rb.*m)();
  return rb.obj.sum != 8;
}

unsigned ::ramfuzz::runtime::spinlimit = 3;
