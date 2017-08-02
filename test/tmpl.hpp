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

/// Tests generation of random instances of template classes.

// Declare A with a differently named parameter -- this used to trick RamFuzz
// into mismatching method names and croulette elements.
template <typename E> class A;

template <typename T> class A {
public:
  A(int) {}
  T plus1(T x) { return x + 1; }
  typedef int INT;
  void m(INT) {}
};

template <int n, typename T> class B {
public:
  B() = default;
};
