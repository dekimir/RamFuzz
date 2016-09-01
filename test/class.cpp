#include "fuzz.hpp"

int main() {
  B b;
  ramfuzz::RF__B rb(b);
  for (auto m : rb.meth_roulette)
    (rb.*m)();
  if (b.sum != 11)
    return 1;
  else
    return 0;
}
