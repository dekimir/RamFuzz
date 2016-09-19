#include "fuzz.hpp"

int main() {
  ramfuzz::runtime::gen g;
  ramfuzz::qqA::control ra(g, 0);
  for (auto m : ra.mroulette)
    (ra.*m)();
  if (ra.obj.sum != 220)
    return 1;
  else
    return 0;
}

unsigned ::ramfuzz::runtime::spinlimit = 3;
