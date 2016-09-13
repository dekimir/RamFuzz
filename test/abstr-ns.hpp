namespace NS {
struct R {
  ~R() {}
};

class A {
public:
  int sum = 40;
  A(int) {}
  A(const R& ) {}
  virtual unsigned f1() = 0;
  virtual void f2(int, double, R& , bool) const = 0;

protected:
  A(double, double) {}

private:
  A(double) {}
  virtual R f3(unsigned) = 0;
};
}

class C {
public:
  int sum = 2;
  void g(const NS::A& a) { sum += a.sum; }
};
