class A {
public:
  int sum = 0;
  A() { sum += 10; }
};

class B {
public:
  int sum = 1;
  void f(A a) { sum += a.sum; }
};
