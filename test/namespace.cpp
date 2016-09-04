#include "fuzz.hpp"

int main() {
  ns2::A a2;
  ramfuzz::ns2::A::control ra2(a2);
  for (auto m : ra2.mroulette)
    (ra2.*m)();
  if (a2.sum != 321)
    return 1;
  ns1::A a1;
  ramfuzz::ns1::A::control ra1(a1);
  for (auto m : ra1.mroulette)
    (ra1.*m)();
  if (a1.sum != 123)
    return 1;
}
