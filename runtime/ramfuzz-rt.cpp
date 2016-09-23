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
using std::size_t;
using std::string;
using std::vector;
using std::uniform_int_distribution;
using std::uniform_real_distribution;

namespace {
template <typename IntegralT>
IntegralT ibetween(IntegralT lo, IntegralT hi, std::ranlux24 &gen) {
  return uniform_int_distribution<IntegralT>{lo, hi}(gen);
}

template <typename RealT>
RealT rbetween(RealT lo, RealT hi, std::ranlux24 &gen) {
  return uniform_real_distribution<RealT>{lo, hi}(gen);
}
} // anonymous namespace

namespace ramfuzz {
namespace runtime {

template <> bool gen::any<bool>() { return ::ibetween(0, 1, rgen); }

template <> int gen::any<int>() {
  return ::ibetween(numeric_limits<int>::min(), numeric_limits<int>::max(),
                    rgen);
}

template <> unsigned gen::any<unsigned>() {
  return ::ibetween(numeric_limits<unsigned>::min(),
                    numeric_limits<unsigned>::max(), rgen);
}

template <> double gen::any<double>() {
  return ::rbetween(numeric_limits<double>::min(),
                    numeric_limits<double>::max(), rgen);
}

template <> float gen::any<float>() {
  return ::rbetween(numeric_limits<float>::min(), numeric_limits<float>::max(),
                    rgen);
}

void gen::set_any(std::vector<bool>::reference obj) { obj = any<bool>(); }

template <> int gen::between<int>(int lo, int hi) {
  return ::ibetween(lo, hi, rgen);
}

template <> size_t gen::between<size_t>(size_t lo, size_t hi) {
  return ::ibetween(lo, hi, rgen);
}

template <> unsigned gen::between<unsigned>(unsigned lo, unsigned hi) {
  return ::ibetween(lo, hi, rgen);
}

template <> int64_t gen::between<int64_t>(int64_t lo, int64_t hi) {
  return ::ibetween(lo, hi, rgen);
}

} // namespace runtime

const qqstdqqexception::control::mptr qqstdqqexception::control::mroulette[] =
    {};
qqstdqqexception::control::control(runtime::gen &g, unsigned ctr) : g(g) {}

} // namespace ramfuzz
