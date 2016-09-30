#include <vector>

struct A {
  std::vector<int> vi;
  std::vector<unsigned> vu;
  std::vector<char> vc;
  std::vector<float> vf;
  void f(int i, char c) {
    vi.push_back(i);
    vc.push_back(c);
  }
  void g(float f, unsigned u) {
    vf.push_back(f);
    vu.push_back(u);
  }
  bool operator!=(const A &that) {
    return vi != that.vi || vu != that.vu || vc != that.vc || vf != that.vf;
  }
};
