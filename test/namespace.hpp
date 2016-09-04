namespace ns1 {
class A {
public:
  int sum = 0;
  void a() { sum += 100; }
  void b() { sum += 20; }
  void c() { sum += 3; }
};
}

namespace ns2 {
class A {
public:
  int sum = 0;
  void a() { sum += 1; }
  void a(int) { sum += 20; }
  void a(bool) { sum += 300; }
};
namespace ns2i {
class A {
public:
  int sum = 0;
  void a() { sum += 45; }
};
} // namespace ns2i
namespace {
class A {
public:
  int sum = 0;
  void a() { sum += 45; }
};
} // anonymous namespace
} // namespace ns2
