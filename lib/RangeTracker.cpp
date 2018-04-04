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

#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "RangeTracker.hpp"

using namespace ramfuzz;
using namespace std;

namespace {

/// Performs Fourier-Motzkin elimination of var in ineqs.  Returns an equivalent
/// set of inequalities without var.
vector<LinearInequality> fomo_step(size_t var,
                                   const vector<LinearInequality> &ineqs) {
  vector<LinearCombination> upper_bounds, lower_bounds;
  vector<LinearInequality> combination;
  for (const auto &current_ineq : ineqs) {
    if (current_ineq.lhs.multipliers.count(var)) {
      const auto b = bound(current_ineq, var);
      if (b.second == Bound::lower)
        lower_bounds.push_back(b.first);
      else
        upper_bounds.push_back(b.first);
    } else
      combination.push_back(current_ineq);
  }
  for (const auto &ub : upper_bounds)
    for (const auto &lb : lower_bounds)
      combination.push_back(
          LinearInequality{ub - lb}); // New inequality: ub >= lb.
  return combination;
}

} // anonymous namespace

namespace ramfuzz {

LinearCombination operator+(const LinearCombination &a,
                            const LinearCombination &b) {
  LinearCombination result(a);
  result.offset += b.offset;
  for (const auto &m : b.multipliers)
    result.multipliers[m.first] += m.second;
  return result;
}

LinearCombination operator-(const LinearCombination &a,
                            const LinearCombination &b) {
  LinearCombination result(a);
  result.offset -= b.offset;
  for (const auto &m : b.multipliers)
    result.multipliers[m.first] -= m.second;
  return result;
}

LinearCombination operator/(const LinearCombination &a, double fac) {
  LinearCombination result;
  result.offset = a.offset / fac;
  for (auto &m : a.multipliers)
    result.multipliers[m.first] = m.second / fac;
  return result;
}

pair<LinearCombination, Bound> bound(const LinearInequality &ineq, size_t var) {
  const auto found = ineq.lhs.multipliers.find(var);
  // (m*var + LHS' >= 0) <=> (var ?? -LHS'/m)
  const auto m = found->second;
  LinearCombination rest_of_lhs(ineq.lhs);
  rest_of_lhs.multipliers.erase(var);
  return make_pair(LinearCombination() - rest_of_lhs / m,
                   m > 0 ? Bound::lower : Bound::upper);
}

pair<double, double> bounds(size_t variable,
                            const vector<LinearInequality> &ineqs) {
  auto lo = numeric_limits<double>::min(), hi = numeric_limits<double>::max();
  if (ineqs.empty())
    return make_tuple(lo, hi);
  // Are there any other variables in ineqs?
  for (const auto &current_ineq : ineqs)
    for (const auto &var_mult : current_ineq.lhs.multipliers) {
      const auto another_variable = var_mult.first;
      if (another_variable != variable) {
        const auto new_ineqs = fomo_step(another_variable, ineqs);
        return bounds(variable, new_ineqs);
      }
    }
  // No other variables -- ineqs dictates variable's bounds.
  for (const auto &current_ineq : ineqs) {
    const auto found = current_ineq.lhs.multipliers.find(variable);
    if (found != current_ineq.lhs.multipliers.end()) {
      const auto m = found->second;
      // (m*x + offset >= 0) <=> (x ?? -offset/m)
      if (m > 0)
        lo = max(lo, -current_ineq.lhs.offset / m);
      else if (m < 0)
        hi = min(hi, -current_ineq.lhs.offset / m);
    }
  }
  return make_pair(lo, hi);
}

} // namespace ramfuzz
