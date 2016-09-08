struct R {
  ~R() {}
};

class A {
public:
  int sum = 21;
  A(int) {}
  A(const R&) {}
  virtual void f1() = 0;
  virtual void f2(int, double, R&, bool) = 0;
protected:
  A(double, double) {}
private:
  A(double) {}
  virtual void f3(unsigned) = 0;
};

class B {
public:
  int sum = 300;
  void g(const A& a) { sum += a.sum; }
};
