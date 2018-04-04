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

#include <unordered_map>
#include <utility>
#include <vector>

namespace ramfuzz {

/// A linear combinatin of multipliers and variables, plus an offset. Each
/// variable is uniquely identified by a size_t number.
class LinearCombination {
public:
  std::unordered_map<size_t, double> multipliers;
  double offset;
};

/// Convenience operator for combining LinearCombinations.
LinearCombination operator+(const LinearCombination &a,
                            const LinearCombination &b);

/// Convenience operator for combining LinearCombinations.
LinearCombination operator-(const LinearCombination &a,
                            const LinearCombination &b);

/// Convenience operator for scaling.
LinearCombination operator/(const LinearCombination &a, double fac);

/// Represents an inequality LHS >= 0, where LHS is a linear combination of
/// variables.
class LinearInequality {
public:
  LinearCombination lhs;

  /// Substitutes value for variable.
  void substitute(size_t variable, double value);
};

/// To distinguish upper and lower bounds.
enum class Bound { upper, lower };

/// Transforms ineq (which must contain a var multiplier) into an equivalent
/// where var is on one side with multiplier 1.0.  Returns the other side, plus
/// an indicator whether that's an upper bound or a lower bound on var.
///
/// For example: if ineq is `4.0*x - 2.0*y + 100.0 >= 0`, then transformed ineq
/// for y is `y <= 2.0*x + 50.0`, yielding a RHS that's an upper bound on y.
std::pair<LinearCombination, Bound> bound(const LinearInequality &ineq,
                                          size_t var);

/// Calculates upper and lower bound for variable's value from ineqs.
std::pair<double, double> bounds(size_t variable,
                                 const std::vector<LinearInequality> &ineqs);

} // namespace ramfuzz
