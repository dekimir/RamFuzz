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

#include "fuzz.hpp"

int main(int argc, char *argv[]) {
  ramfuzz::runtime::gen g(argc, argv);
  ns2::A a2;
  ramfuzz::harness<ns2::A> ra2(g, a2);
  for (auto m : ra2.mroulette)
    (ra2.*m)();
  if (a2.sum != 321)
    return 1;
  ns1::A a1;
  ramfuzz::harness<ns1::A> ra1(g, a1);
  for (auto m : ra1.mroulette)
    (ra1.*m)();
  if (a1.sum != 123)
    return 1;
  ns2::ns2i::A a2i;
  ramfuzz::harness<ns2::ns2i::A> ra2i(g, a2i);
  for (auto m : ra2i.mroulette)
    (ra2i.*m)();
  if (a2i.sum != 45)
    return 1;
}

unsigned ::ramfuzz::runtime::spinlimit = 3;
