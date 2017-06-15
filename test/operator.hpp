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

/// Tests handling of various operator methods.

namespace ns1 {
  struct A {
    int a = 123;
  };
}

//template<typename T>
//class templ {};

namespace ns2 {
  struct B {
    operator ns1::A() { return ns1::A(); }
    //operator templ<B>() { return templ<B>(); }
  };
}
