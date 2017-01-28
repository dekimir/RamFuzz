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

#include <vector>

struct A {
  std::vector<int> vi;
  std::vector<unsigned> vu;
  std::vector<char> vc;
  std::vector<float> vf;
  void f(int i, char c) {
    vi.push_back(i);
    vc.push_back(c);
  }
  void g(float f, const std::vector<unsigned> u) {
    vf.push_back(f);
    vu.insert(vu.cbegin(), u.cbegin(), u.cend());
  }
  bool operator!=(const A &that) {
    return vi != that.vi || vu != that.vu || vc != that.vc || vf != that.vf;
  }
};
