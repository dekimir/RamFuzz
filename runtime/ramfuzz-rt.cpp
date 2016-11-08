// Copyright 2016 The RamFuzz contributors. All rights reserved.
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
#include <iostream>
#include <limits>

using std::cout;
using std::endl;
using std::generate;
using std::hex;
using std::isprint;
using std::istream;
using std::numeric_limits;
using std::ofstream;
using std::ranlux24;
using std::size_t;
using std::streamsize;
using std::string;
using std::vector;
using std::uniform_int_distribution;
using std::uniform_real_distribution;

namespace {

template <typename IntegralT>
IntegralT ibetween(IntegralT lo, IntegralT hi, ranlux24 &gen) {
  return uniform_int_distribution<IntegralT>{lo, hi}(gen);
}

template <typename RealT> RealT rbetween(RealT lo, RealT hi, ranlux24 &gen) {
  return uniform_real_distribution<RealT>{lo, hi}(gen);
}

} // anonymous namespace

namespace ramfuzz {
namespace runtime {

template <> bool gen::uniform_random<bool>(bool lo, bool hi) {
  return ibetween(lo, hi, rgen);
}

template <> double gen::uniform_random<double>(double lo, double hi) {
  return rbetween(lo, hi, rgen);
}

template <> float gen::uniform_random<float>(float lo, float hi) {
  return rbetween(lo, hi, rgen);
}

void gen::set_any(std::vector<bool>::reference obj) { obj = any<bool>(); }

template <> int gen::uniform_random<int>(int lo, int hi) {
  return ibetween(lo, hi, rgen);
}

template <> size_t gen::uniform_random<size_t>(size_t lo, size_t hi) {
  return ibetween(lo, hi, rgen);
}

template <> unsigned gen::uniform_random<unsigned>(unsigned lo, unsigned hi) {
  return ibetween(lo, hi, rgen);
}

template <> int64_t gen::uniform_random<int64_t>(int64_t lo, int64_t hi) {
  return ibetween(lo, hi, rgen);
}

template <> char gen::uniform_random<char>(char lo, char hi) {
  return ibetween(lo, hi, rgen);
}

template <>
unsigned char gen::uniform_random<unsigned char>(unsigned char lo,
                                                 unsigned char hi) {
  return ibetween(lo, hi, rgen);
}

gen::skip::skip(istream &str) : valid(false) {
  if (!(str >> start_))
    return;
  if (!(str >> end_)) {
    std::cerr << "warning: no end position in control file\n";
    return;
  }
  valid = true;
  str.ignore(numeric_limits<streamsize>::max(), '\n');
}

} // namespace runtime

const rfstd_exception::control::mptr rfstd_exception::control::mroulette[] = {};

rfstd_exception::control::control(runtime::gen &g, unsigned ctr) : g(g) {}

} // namespace ramfuzz
