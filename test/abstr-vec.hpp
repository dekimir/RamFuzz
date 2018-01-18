// Copyright 2016-2018 The RamFuzz contributors. All rights reserved.
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

#include <vector>

namespace NS {
struct Element {
  void e() {}
};

struct S {
  typedef int I;
};

template <typename T> struct ST { T t; };

class A {
  int sum = 3;

public:
  A(const std::vector<double> &) {}
  int get() const { return sum; }
  virtual std::vector<unsigned> f(const std::vector<bool> *) = 0;
  virtual void g1(std::vector<Element>) = 0;
  virtual void g2(const std::vector<Element>) = 0;
  virtual void g3(std::vector<Element> &) = 0;
  virtual void g4(const std::vector<Element> &) = 0;
  virtual void g5(std::vector<Element> *) = 0;
  virtual void g6(const std::vector<Element> *) = 0;
  virtual S::I h1(std::vector<S::I> &) = 0;
  S::I h2(std::vector<S::I> &) { return 123; }
  virtual void f1(std::vector<ST<int>>) = 0;
};
}

using std::vector;
class B {
public:
  const int m = 1;
  virtual int s(const vector<int> &) = 0;
};

class C {
  int sum = 0x33;

public:
  int get() const { return sum; }
  void g(const NS::A &a, B &b) { sum &= a.get() * b.m; }
};

#include "ramfuzz-rt.hpp"
template <> NS::ST<int> *ramfuzz::runtime::gen::make<NS::ST<int>>(bool);
