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

struct R {
  ~R() {}
};

class A {
public:
  int sum = 21;
  A(int) {}
  A(const R&) {}
  virtual unsigned f1() = 0;
  virtual void f2(int, double, R&, bool) const = 0;
protected:
  A(double, double) {}
private:
  A(double) {}
  virtual R f3(unsigned) = 0;
};

struct B {
  int sum = 4000;
  virtual float g(bool) = 0;
};

class C {
public:
  int sum = 300;
  void g(const A& a) { sum += a.sum; }
  void g(const B& b) { sum += b.sum; }
};
