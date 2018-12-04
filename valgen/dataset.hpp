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

/// Returns a tensor of the n-edge path ending in e.  If there are fewer than n
/// edges between root and e, pads the tensor with zeros on the right.
torch::Tensor last_n(const edge* e, size_t n);

}  // namespace exetree
}  // namespace ramfuzz
