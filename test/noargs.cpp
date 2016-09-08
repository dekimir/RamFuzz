#include "fuzz.hpp"

int main() {
  ramfuzz::runtime::gen g;
  C c;
  ramfuzz::C::control rc(g, c);
  for (auto m : rc.mroulette)
    (rc.*m)();
  if (c.sum != 123)
    return 1;
  else
    return 0;
}
