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

#include <torch/nn.h>
#include <memory>

#include "exetree.hpp"

namespace ramfuzz {

class valgen_nnet : public torch::nn::Module {
 public:
  valgen_nnet();

  /// Incrementally trains *this with the root corpus.
  void train_more(const exetree::node& root);

  /// Returns the neural network's output on vals.
  torch::Tensor forward(const torch::Tensor& vals);

  /// Translates a valgen_nnet output into a simple bool: true iff prediction
  /// means the input node may reach successful termination.
  static bool prediction_as_bool(const torch::Tensor& prediction) {
    return prediction[0].item<double>() > prediction[1].item<double>();
  }

  /// Opposite of prediction_as_bool.
  static torch::Tensor bool_as_prediction(bool maywin) {
    return torch::tensor({maywin, !maywin}, at::kDouble);
  }

  bool predict(const torch::Tensor& in) {
    return prediction_as_bool(forward(in));
  }

 private:
  torch::nn::Linear lin;
};

}  // namespace ramfuzz
