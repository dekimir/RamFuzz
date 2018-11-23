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

#pragma once

#include <torch/data/dataloader.h>
#include <torch/data/datasets.h>
#include <torch/data/samplers.h>
#include <memory>

#include "exetree.hpp"

namespace ramfuzz {
namespace exetree {

/// Turns an execution tree into a torch dataset.
class ExeTreeDataset : public torch::data::datasets::Dataset<ExeTreeDataset> {
 public:
  explicit ExeTreeDataset(const node& root);
  torch::data::Example<> get(size_t index) override;
  torch::optional<size_t> size() const override { return _size; };

 private:
  dfs_cursor current;
  size_t last_index;  ///< Records get() argument at last invocation.
  size_t _size;
};

std::unique_ptr<torch::data::DataLoader<
    ExeTreeDataset, torch::data::samplers::SequentialSampler>>
make_data_loader(const node& n, size_t batch_size = 64);

}  // namespace exetree
}  // namespace ramfuzz
