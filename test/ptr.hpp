class A {
public:
  int num = -12;
  A() : num(3) {}
};

class B {
public:
  int sum = 0;
  void f(int *p, A *a) { sum += bool(p) * a->num; }
};
