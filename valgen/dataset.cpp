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

ExeTreeDataset::ExeTreeDataset(const node& root)
    : current(root), last_index(0), _size(0) {
  for (auto c = dfs_cursor(root); c; ++c) ++_size;
}

Example<> ExeTreeDataset::get(size_t index) {
  assert(index == last_index + 1 || (index == 0 && last_index == 0));
  last_index = index;
  double value = *current;
  bool label = current->dst()->maywin();
  ++current;
  return Example<>(torch::tensor({value}), torch::tensor(label));
}

unique_ptr<DataLoader<ExeTreeDataset, samplers::SequentialSampler>>
make_data_loader(const node& n, size_t batch_size) {
  ExeTreeDataset ds(n);
  return torch::data::make_data_loader(ds, batch_size,
                                       samplers::SequentialSampler(*ds.size()));
}

}  // namespace exetree
}  // namespace ramfuzz
