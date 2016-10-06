#include "fuzz.hpp"

int main() {
  ramfuzz::runtime::gen g;
  ns2::A a2;
  ramfuzz::rfns2_A::control ra2(g, a2);
  for (auto m : ra2.mroulette)
    (ra2.*m)();
  if (a2.sum != 321)
    return 1;
  ns1::A a1;
  ramfuzz::rfns1_A::control ra1(g, a1);
  for (auto m : ra1.mroulette)
    (ra1.*m)();
  if (a1.sum != 123)
    return 1;
  ns2::ns2i::A a2i;
  ramfuzz::rfns2_ns2i_A::control ra2i(g, a2i);
  for (auto m : ra2i.mroulette)
    (ra2i.*m)();
  if (a2i.sum != 45)
    return 1;
}

unsigned ::ramfuzz::runtime::spinlimit = 3;
