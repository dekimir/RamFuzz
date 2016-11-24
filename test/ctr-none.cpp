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

#include <memory>

#include "fuzz.hpp"

/// Tests that correct code is generated when the class under test has no public
/// constructors.

int main() {
  // The control class must exist, but it can't be constructed.
  ramfuzz::rfB::control *ctl;
}

ramfuzz::rfB::control ramfuzz::rfB::control::make(ramfuzz::runtime::gen &g) {
  std::unique_ptr<B> b(B::create());
  control c(g, *b);
  c.pobj = std::move(b);
  return c;
}

template <> void ramfuzz::runtime::gen::set_any<B>(B *&pb) { pb = nullptr; }

unsigned ::ramfuzz::runtime::spinlimit = 3;
