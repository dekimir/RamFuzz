#include "fuzz.hpp"

int main() {
  C c;
  ramfuzz::RF__C rc(c);
  for (auto m : rc.mroulette)
    (rc.*m)();
  if (c.sum != 10)
    return 1;
  else
    return 0;
}
