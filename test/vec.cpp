#include "fuzz.hpp"

int main() {
  ramfuzz::runtime::gen g;
  ramfuzz::ns1::A::control ra1(g, 0);
  for (auto m : ra1.mroulette)
    (ra1.*m)();
  return (ra1.obj.sum != 3);
}

unsigned ::ramfuzz::runtime::spinlimit = 3;
