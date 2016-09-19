#include "fuzz.hpp"

int main() {
  ramfuzz::runtime::gen g;
  ramfuzz::qqB::control rb(g, ramfuzz::qqB::control::ccount - 1);
  for (auto m : rb.mroulette)
    (rb.*m)();
  return rb.obj.sum != 8;
}

unsigned ::ramfuzz::runtime::spinlimit = 3;
