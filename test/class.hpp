class A { public: A(){} };
class B {
public:
  int sum = 0;
  void f(A) { sum += 11; }
};
