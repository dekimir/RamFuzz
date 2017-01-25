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

/// Tests subclass rams when class hierarchy is deep.

class Base {
public:
  virtual int id() const { return 0xba0; }
  bool operator=(const Base &) = delete;
};

class Sub1 : public Base {
  int id() const override { return 0x5c1; }
};

class Sub2 : public Sub1 {
  int id() const override { return 0x5c2; }
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
