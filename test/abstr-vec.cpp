#include "fuzz.hpp"

int main() {
  ramfuzz::runtime::gen g;
  ramfuzz::rfC::control rc(g, 0);
  for (auto m : rc.mroulette)
    (rc.*m)();
  return (rc.obj.sum != 30);
}

unsigned ::ramfuzz::runtime::spinlimit = 3;
