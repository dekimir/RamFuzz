// Copyright 2016 Heavy Automation Limited.  All rights reserved.

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
using std::numeric_limits;
using std::ofstream;
using std::ranlux24;
using std::size_t;
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

} // namespace runtime

const rfstd_exception::control::mptr rfstd_exception::control::mroulette[] = {};

rfstd_exception::control::control(runtime::gen &g, unsigned ctr) : g(g) {}

} // namespace ramfuzz
