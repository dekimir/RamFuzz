#include <vector>
namespace ns1 {
class A {
public:
  int sum = 0;
  void a(std::vector<int>) { sum += 3; }
};
} // namespace ns1
