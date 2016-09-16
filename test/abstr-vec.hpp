#include <vector>
using std::vector;

namespace NS {
class A {
public:
  int sum = 3;
  A(const vector<double> &) {}
  virtual vector<unsigned> f(const vector<bool> *) = 0;
};
}

class C {
public:
  int sum = 33;
  void g(const NS::A &a) { sum -= a.sum; }
};
