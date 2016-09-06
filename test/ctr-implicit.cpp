#include "fuzz.hpp"

int main() {
  ramfuzz::B::control rb(ramfuzz::B::control::ccount - 1);
  for (auto m : rb.mroulette)
    (rb.*m)();
  return rb.obj.sum != 8;
}
