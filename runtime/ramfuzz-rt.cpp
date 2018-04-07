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

#include "ramfuzz-rt.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <tuple>

using namespace std;

namespace {

using namespace ramfuzz::runtime;

template <typename IntegralT>
IntegralT ibetween(IntegralT lo, IntegralT hi, ranlux24 &gen) {
  return uniform_int_distribution<IntegralT>{lo, hi}(gen);
}

template <typename RealT> RealT rbetween(RealT lo, RealT hi, ranlux24 &gen) {
  return uniform_real_distribution<RealT>{lo, hi}(gen);
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

/// To distinguish upper and lower bounds.
enum class Bound { upper, lower };

pair<LinearCombination, Bound> bound(const LinearInequality &ineq, size_t var) {
  const auto found = ineq.lhs.multipliers.find(var);
  // (m*var + LHS' >= 0) <=> (var ?? -LHS'/m)
  const auto m = found->second;
  LinearCombination rest_of_lhs(ineq.lhs);
  rest_of_lhs.multipliers.erase(var);
  return make_pair(LinearCombination() - rest_of_lhs / m,
                   m > 0 ? Bound::lower : Bound::upper);
}

/// Performs Fourier-Motzkin elimination of var in ineqs.  Returns an equivalent
/// set of inequalities without var.
vector<LinearInequality> fomo_step(size_t var,
                                   const vector<LinearInequality> &ineqs) {
  vector<LinearCombination> upper_bounds, lower_bounds;
  vector<LinearInequality> combination;
  for (const auto &current_ineq : ineqs) {
    const auto found = current_ineq.lhs.multipliers.find(var);
    if (found == current_ineq.lhs.multipliers.end())
      combination.push_back(current_ineq);
    else if (found->second == 0.) {
      combination.push_back(current_ineq);
      combination.back().lhs.multipliers.erase(var);
    } else {
      const auto b = bound(current_ineq, var);
      if (b.second == Bound::lower)
        lower_bounds.push_back(b.first);
      else
        upper_bounds.push_back(b.first);
    }
  }
  for (const auto &ub : upper_bounds)
    for (const auto &lb : lower_bounds)
      combination.push_back(
          LinearInequality{ub - lb}); // New inequality: ub >= lb.
  return combination;
}

tuple<double, double> bounds(size_t variable,
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

} // anonymous namespace

namespace ramfuzz {
namespace runtime {

gen::gen(const string &ologname) : runmode(generate), olog(ologname) {
  if (!olog)
    throw file_error("Cannot open " + ologname);
}

gen::gen(const string &ilogname, const string &ologname)
    : runmode(replay), olog(ologname), ilog(ilogname) {
  if (!olog)
    throw file_error("Cannot open " + ologname);
  if (!ilog)
    throw file_error("Cannot open " + ilogname);
}

gen::gen(int argc, const char *const *argv, size_t k) {
  size_t argnum = static_cast<size_t>(argc);
  if (k < argnum && argv[k]) {
    runmode = replay;
    const string argstr(argv[k]);
    ilog.open(argstr);
    if (!ilog)
      throw file_error("Cannot open " + argstr);
    olog.open(argstr + "+");
    if (!olog)
      throw file_error("Cannot open " + argstr + "+");
    if (k + 1 < argnum && argv[k + 1]) {
      countdown = atol(argv[k + 1]);
      counting = true;
    }
  } else {
    runmode = generate;
    olog.open("fuzzlog");
    if (!olog)
      throw file_error("Cannot open fuzzlog");
  }
}

template <> bool gen::uniform_random<bool>(bool lo, bool hi) {
  return uniform_int_distribution<int>{lo, hi}(rgen);
}

template <> double gen::uniform_random<double>(double lo, double hi) {
  return rbetween(lo, hi, rgen);
}

template <> float gen::uniform_random<float>(float lo, float hi) {
  return rbetween(lo, hi, rgen);
}

// Depending on your C++ implementation, some of the below definitions may have
// to be commented out or uncommented.  Implementations typically define certain
// integer types to be identical (eg, size_t and unsigned long), so you can't
// keep both definitions.  The code below compiles successfully with MacOS
// clang; that requires commenting out some integer types that are identical to
// uncommented ones.  But your implementation may differ.
//
// If you get an error like "redefinition of uniform_random" for a type, just
// comment out that type's definition.  If, OTOH, you get an error like
// "undefined symbol uniform_random" for a type, then uncomment that type's
// definition.

template <> short gen::uniform_random<short>(short lo, short hi) {
  return ibetween(lo, hi, rgen);
}

template <>
unsigned short gen::uniform_random<unsigned short>(unsigned short lo,
                                                   unsigned short hi) {
  return ibetween(lo, hi, rgen);
}

template <> int gen::uniform_random<int>(int lo, int hi) {
  return ibetween(lo, hi, rgen);
}

template <> unsigned gen::uniform_random<unsigned>(unsigned lo, unsigned hi) {
  return ibetween(lo, hi, rgen);
}

template <> long gen::uniform_random<long>(long lo, long hi) {
  return ibetween(lo, hi, rgen);
}

template <>
unsigned long gen::uniform_random<unsigned long>(unsigned long lo,
                                                 unsigned long hi) {
  return ibetween(lo, min(hi, 500UL), rgen);
}

template <>
long long gen::uniform_random<long long>(long long lo, long long hi) {
  return ibetween(lo, hi, rgen);
}

template <>
unsigned long long
gen::uniform_random<unsigned long long>(unsigned long long lo,
                                        unsigned long long hi) {
  return ibetween(lo, hi, rgen);
}
/*
template <> size_t gen::uniform_random<size_t>(size_t lo, size_t hi) {
  return ibetween(lo, hi, rgen);
}

template <> int64_t gen::uniform_random<int64_t>(int64_t lo, int64_t hi) {
  return ibetween(lo, hi, rgen);
}
*/
template <> char gen::uniform_random<char>(char lo, char hi) {
  return ibetween(lo, hi, rgen);
}

template <>
unsigned char gen::uniform_random<unsigned char>(unsigned char lo,
                                                 unsigned char hi) {
  return ibetween(lo, hi, rgen);
}

template <> char typetag<bool>(bool) { return 0; }
template <> char typetag<char>(char) { return 1; }
template <> char typetag<unsigned char>(unsigned char) { return 2; }
template <> char typetag<short>(short) { return 3; }
template <> char typetag<unsigned short>(unsigned short) { return 4; }
template <> char typetag<int>(int) { return 5; }
template <> char typetag<unsigned int>(unsigned int) { return 6; }
template <> char typetag<long>(long) { return 7; }
template <> char typetag<unsigned long>(unsigned long) { return 8; }
template <> char typetag<long long>(long long) { return 9; }
template <> char typetag<unsigned long long>(unsigned long long) { return 10; }
template <> char typetag<float>(float) { return 11; }
template <> char typetag<double>(double) { return 12; }

void LinearInequality::substitute(size_t variable, double value) {
  const auto found = lhs.multipliers.find(variable);
  if (found == lhs.multipliers.end())
    return;
  lhs.offset += found->second * value;
  lhs.multipliers.erase(found);
}

size_t gen::random_value(size_t lo, size_t hi, size_t valueid) {
  const bool is_restricted = find(begin(restricted_ids), end(restricted_ids),
                                  valueid) == end(restricted_ids);
  if (is_restricted)
    tie(lo, hi) = bounds(valueid, current_constraints);
  else
    current_constraints = starting_constraints;
  size_t val = uniform_random(lo, hi);
  if (is_restricted)
    for (auto &i : current_constraints)
      i.substitute(valueid, val);
  return 0; // val;
}

} // namespace runtime
} // namespace ramfuzz
