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

// Tests a class with a constructor but no safe constructors.  Ensures that a
// depth-limit breach doesn't cause a null dereference.

class B;

class A {
public:
  int id;
  A() { id = 123; }
  A(B &b);
  A(B *b);
};

class B {
public:
  int id;
  B(A a) { id = a.id; }
};
