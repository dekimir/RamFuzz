// Copyright 2016 The RamFuzz contributors. All rights reserved.
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

/// Tests classes that inherit from abstract base classes.

class A1 {
public:
  virtual void f1(int, double, bool) const = 0;

protected:
  virtual int f2(unsigned) = 0;
};

class B1 : public A1 {
public:
  virtual float g1(bool) = 0;
  virtual int f2(unsigned) = 0;
};

class C1 : public B1 {
public:
  int sum = 300;
};

class A2 {
public:
  virtual void h1(int, double, bool) const = 0;

protected:
  virtual int h2(unsigned) = 0;
};

class B2 : public A2 {};

class C2 : public B2, public B1 {
public:
  int sum = 4000;
};

class D {
public:
  int sum = 21;
  D(C1 &c1, C2 &c2) { sum += c1.sum + c2.sum; }

private:
  D(const D &) = delete;
};
