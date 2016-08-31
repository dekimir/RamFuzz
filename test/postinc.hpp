class C {
public:
  int sum = 0;
  void operator++(int) { sum += 10; }
};
