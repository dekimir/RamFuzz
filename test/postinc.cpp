#include "fuzz.hpp"

int main() {
  ramfuzz::runtime::gen g;
  C c;
  ramfuzz::rfC::control rc(g, c);
  for (auto m : rc.mroulette)
    (rc.*m)();
  return (c.sum != 10);
}

unsigned ::ramfuzz::runtime::spinlimit = 3;
