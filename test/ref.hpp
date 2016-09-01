class A {
public:
  A() {}
  int sum = 0;
  void f(int&, const unsigned&) { sum += 20; }
  void g(A&, const A&) { sum += 200; }
};
