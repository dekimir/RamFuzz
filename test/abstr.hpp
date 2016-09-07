class A {
public:
  int sum = 21;
  A(int) {}
  A(const A&) {}
  virtual void f1() = 0;
  virtual void f2(int, double, A&, bool) = 0;
};

class B {
public:
  int sum = 300;
  void g(const A& a) { sum += a.sum; }
};
