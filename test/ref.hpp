struct R {
  ~R() {}
};

class A {
public:
  A() {}
  int sum = 0;
  void f(int&, const unsigned&) { sum += 20; }
  void g(R&, const R&) { sum += 200; }
};
