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

#include <vector>
namespace ns1 {
class A {
public:
  int f() const { return 10; }
};
} // namespace ns1

namespace ns2 {
class Abst {
public:
  virtual void f() = 0;
};
} // namespace ns2

using ns2::Abst;

class B {
  int sum = 5;

public:
  int get() const { return sum; }
  void f1(const std::vector<ns1::A *> &v) {
    if (!v.empty())
      sum = v[0]->f() / 2;
  }
  void f2(const std::vector<int *> &v) {
    if (!v.empty())
      sum += *v[0];
    if (!v.empty())
      sum -= *v[0];
  }
  void f3(const std::vector<void *> &v) {}
  void f4(const std::vector<Abst *> &v) {}
  void f5(const std::vector<const int *> &v) {}
  void f6(std::vector<const ns1::A *> v) {}
};
