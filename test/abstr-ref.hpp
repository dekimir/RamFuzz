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

/// Tests pure methods returning a reference.

class A {
  int sum = 11;

public:
  int get() const { return sum; }
  virtual int *f1(unsigned) = 0;
  virtual A &f2() = 0;
};

class B {
  int sum = 22;

public:
  int get() const { return sum; }
  void g(A &a) { sum = 22 + a.get() * 100; }
};
