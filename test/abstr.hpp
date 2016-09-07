class A {
public:
  int sum = 21;
  virtual void f() = 0;
};

class B {
public:
  int sum = 300;
  void g(A a) { sum += a.sum; }
};
