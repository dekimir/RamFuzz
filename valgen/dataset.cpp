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

#include "dataset.hpp"

#include <cassert>

using namespace std;
using namespace torch::data;

namespace ramfuzz {
namespace exetree {

torch::Tensor last_n(const edge* e, size_t n) {
  auto values = torch::zeros(n, at::kDouble);
  auto edge_ptr = e;
  auto i = n;
  while (i > 0 && edge_ptr) {
    --i;
    values[i] = double(*edge_ptr);
    edge_ptr = edge_ptr->src()->incoming_edge();
  }
  if (i == 0) return values;
  const auto spl = values.split_with_sizes({int64_t(i), int64_t(n - i)});
  return torch::cat({spl[1], spl[0]});
}

}  // namespace exetree
}  // namespace ramfuzz