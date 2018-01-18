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

/// Tests templates in a namespace.

namespace ns1 {

template <typename T> class A {
public:
  A() = default;
  A(const A &) {}
  T plus1(T x) { return x + 1; }
};

class B {
  A<unsigned> &a;

public:
  B(A<unsigned> &a) : a(a) {}
  unsigned get() const { return a.plus1(122); }
};

} // namespace ns1
