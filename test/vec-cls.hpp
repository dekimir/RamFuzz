#include <vector>
namespace ns1 {

class A {
public:
  int f() const { return 10; }
};

class B {
public:
  int sum = 0;
  void g(const std::vector<A> &v) { sum += v.size() ? v[0].f() - 5 : 5; }
};

} // namespace ns1
