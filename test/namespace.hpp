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

namespace ns1 {
typedef int Int;
class A {
  int sum = 0;

public:
  int get() const { return sum; }
  void a(Int *) { sum |= 0x100; }
  void b() { sum |= 0x20; }
  void c() { sum |= 0x3; }
};
}

namespace ns2 {
class A {
  int sum = 0;

public:
  int get() const { return sum; }
  void a() { sum |= 0x1; }
  void a(int) { sum |= 0x20; }
  void a(bool) { sum |= 0x300; }
};
namespace ns2i {
class A {
  int sum = 0;

public:
  int get() const { return sum; }
  void a() { sum |= 0x45; }
};
} // namespace ns2i
namespace {
class A {
  int sum = 0;

public:
  int get() const { return sum; }
  void a() { sum |= 0x45; }
};
} // anonymous namespace
} // namespace ns2
