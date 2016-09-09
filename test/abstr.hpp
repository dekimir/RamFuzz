struct R {
  ~R() {}
};

class A {
public:
  int sum = 21;
  A(int) {}
  A(const R&) {}
  virtual unsigned f1() = 0;
  virtual void f2(int, double, R&, bool) = 0;
protected:
  A(double, double) {}
private:
  A(double) {}
  virtual void f3(unsigned) = 0;
};

struct B {
  int sum = 4000;
  virtual float f(bool) = 0;
};

class C {
public:
  int sum = 300;
  void g(const A& a) { sum += a.sum; }
  void g(const B& b) { sum += b.sum; }
};
