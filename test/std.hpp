#include <exception>

namespace ns1 {
class A {
public:
  int sum = 0;
  void a(std::exception& e) { sum += 56; }
};
} // namespace ns1
