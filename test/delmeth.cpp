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

int main(int argc, char* argv[]) {
  ramfuzz::runtime::gen g(argc, argv);
  ramfuzz::rfB::control rb(g, ramfuzz::rfB::control::ccount - 1);
  for (auto m : rb.mroulette)
    (rb.*m)();
  ramfuzz::rfC::control rc(g, ramfuzz::rfC::control::ccount - 1);
  for (auto m : rc.mroulette)
    (rc.*m)();
  return rb.obj.sum != 42 || rc.obj.sum != 43;
}

unsigned ::ramfuzz::runtime::spinlimit = 3;
