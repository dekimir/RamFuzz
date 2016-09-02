#include "fuzz.hpp"

int main() {
  B b;
  ramfuzz::RF__B rb(b);
  for (auto m : rb.mroulette)
    (rb.*m)();
  if (b.sum != 11)
    return 1;
  else
    return 0;
}

unsigned ::ramfuzz::runtime::spinlimit = 3;
