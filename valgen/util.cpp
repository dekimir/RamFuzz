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

#include "util.hpp"

using namespace std;

namespace ramfuzz {

torch::Tensor pad_right(const vector<double>& v, size_t sz) {
  assert(v.size() <= sz);
  auto wider = torch::zeros(sz, at::kDouble);
  for (size_t i = 0; i < v.size(); ++i) wider[i] = v[i];
  return wider;
}

}  // namespace ramfuzz
