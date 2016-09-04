#include "fuzz.hpp"

int main() {
  C c;
  ramfuzz::C::control rc(c);
  for (auto m : rc.mroulette)
    (rc.*m)();
  if (c.sum != 32)
    return 1;
  else
    return 0;
}
