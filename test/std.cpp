#include "fuzz.hpp"

int main() {
  ramfuzz::runtime::gen g;
  ramfuzz::rfns1_A::control ra1(g, 0);
  for (auto m : ra1.mroulette)
    (ra1.*m)();
  return (ra1.obj.sum != 56);
}

unsigned ::ramfuzz::runtime::spinlimit = 3;
