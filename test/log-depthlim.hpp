#include <vector>

struct A {
  std::vector<int> vi;
  void depth(int i, A &a) { vi.push_back(i); }
  void depth(A &a) { vi = a.vi; }
  void depth(A &a, unsigned u) { vi.push_back(int(u)); }
  bool operator!=(const A &that) { return vi != that.vi; }
};
