#include "fuzz.hpp"

int main() {
  ramfuzz::runtime::gen g;
  ramfuzz::rfns1_B::control rb1(g, 0);
  for (auto m : rb1.mroulette)
    (rb1.*m)();
  return (rb1.obj.sum != 5);
}

unsigned ::ramfuzz::runtime::spinlimit = 3;
