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

/// Tests using subclass rams.

class Base {
public:
  virtual int id() const { return 0xba; }
  int m1(Base &b) const { return b.id(); }
  int m2(Base *b) const { return b->id(); }
  int m3(Base b) const { return b.id(); }
};

class Subcl : public Base {
  int id() const { return 0x5c; }
};

class ClientByRef {
public:
  int trace = 0;
  void m(const Base &b) { trace |= b.id(); }
};

class ClientByPtr {
public:
  int trace = 0;
  void m(const Base *b) { trace |= b->id(); }
};
