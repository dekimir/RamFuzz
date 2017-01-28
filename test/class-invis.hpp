// Copyright 2016-2017 The RamFuzz contributors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/// This is much like class.hpp, except it includes many classes that can't have
/// a RamFuzz control because they're not visible outside their immediate
/// context.  Trying to declare a RamFuzz control for such classes will result
/// in compilation errors.

class A {
  class A2 {
  public:
    int foo(char) { return 0; }
  };
  template <typename T> class TA2 {
  public:
    T foo(char) { return 0; }
  };

public:
  int sum = 0;
  A() { sum += 10; }
  TA2<double> ta2;

  class A3 {
  private:
    struct A4 {
      struct A5 {
        class A6 {
        public:
          int foo(char) { return 1; }
        };
        template <typename T> class TA6 {
        public:
          T foo(char) { return 1; }
        };
      };
    };

  public:
    A4::A5::TA6<float> ta6;
  };

  class A7 {
  public:
    const int one() const { return 1; }
  };
};

class B {
public:
  int sum = 1;
  void f(A a, const A::A7 &a7) { sum += a.sum * a7.one(); }
};

namespace NS1 {
namespace NS2 {
namespace {
class C {
public:
  void f1(bool) {}
};
template <typename T> class TC {
public:
  void f1(T) {}
};
namespace NS3 {
class C2 {
public:
  void g1(bool) {}
};
template <typename T> class TC2 {
public:
  void g1(T) {}
};
} // namespace NS3
} // anonymous namespace
TC<int> tc;
NS3::TC2<bool> tc2;
} // namespace NS2
} // namespace NS1

inline void f2() {
  class D {
  public:
    void f3(int) {}
  };
}

class {
public:
  char f4(char c) { return c; }
} e;
