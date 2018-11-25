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

namespace {

using namespace ramfuzz::exetree;

/// Returns a tensor of the n-edge path ending in e.  If there are fewer than n
/// edges between root and e, pads the tensor with zeros.
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

}  // namespace

namespace ramfuzz {
namespace exetree {

ExeTreeDataset::ExeTreeDataset(const node& root)
    : current(root), last_index(0), _size(0) {
  for (auto c = dfs_cursor(root); c; ++c) ++_size;
}

Example<> ExeTreeDataset::get(size_t index) {
  assert(index == last_index + 1 || (index == 0 && last_index == 0));
  last_index = index;
  auto values = last_n(&*current, 10);
  bool label = current->dst()->maywin();
  ++current;
  return Example<>(values, torch::tensor(label));
}

unique_ptr<DataLoader<ExeTreeDataset, samplers::SequentialSampler>>
make_data_loader(const node& n, size_t batch_size) {
  ExeTreeDataset ds(n);
  return torch::data::make_data_loader(ds, batch_size,
                                       samplers::SequentialSampler(*ds.size()));
}

}  // namespace exetree
}  // namespace ramfuzz
