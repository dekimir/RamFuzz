#include <vector>

struct A {
  std::vector<int> vi;
  void f(int i) { vi.push_back(i); }
};
