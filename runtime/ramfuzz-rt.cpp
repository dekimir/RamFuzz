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
IntegralT ibetween(IntegralT lo, IntegralT hi, std::ranlux24 &gen, long val_id,
                   long sub_id) {
  const auto val = uniform_int_distribution<IntegralT>{lo, hi}(gen);
  cout << '(' << val_id << ',' << sub_id << ")=" << val << endl;
  return val;
}

template <typename RealT>
RealT rbetween(RealT lo, RealT hi, std::ranlux24 &gen, long val_id,
               long sub_id) {
  const auto val = uniform_real_distribution<RealT>{lo, hi}(gen);
  cout << '(' << val_id << ',' << sub_id << ")=" << val << endl;
  return val;
}
} // anonymous namespace

namespace ramfuzz {
namespace runtime {

template <> bool gen::any<bool>(long val_id, long sub_id) {
  return ::ibetween(0, 1, rgen, val_id, sub_id);
}

template <> int gen::any<int>(long val_id, long sub_id) {
  return ::ibetween(numeric_limits<int>::min(), numeric_limits<int>::max(),
                    rgen, val_id, sub_id);
}

template <> unsigned gen::any<unsigned>(long val_id, long sub_id) {
  return ::ibetween(numeric_limits<unsigned>::min(),
                    numeric_limits<unsigned>::max(), rgen, val_id, sub_id);
}

template <> double gen::any<double>(long val_id, long sub_id) {
  return ::rbetween(numeric_limits<double>::min(),
                    numeric_limits<double>::max(), rgen, val_id, sub_id);
}

template <> float gen::any<float>(long val_id, long sub_id) {
  return ::rbetween(numeric_limits<float>::min(), numeric_limits<float>::max(),
                    rgen, val_id, sub_id);
}

void gen::set_any(std::vector<bool>::reference obj, long val_id, long sub_id) {
  obj = any<bool>(val_id, sub_id);
}

template <> int gen::between<int>(int lo, int hi, long val_id, long sub_id) {
  return ::ibetween(lo, hi, rgen, val_id, sub_id);
}

template <>
size_t gen::between<size_t>(size_t lo, size_t hi, long val_id, long sub_id) {
  return ::ibetween(lo, hi, rgen, val_id, sub_id);
}

template <>
unsigned gen::between<unsigned>(unsigned lo, unsigned hi, long val_id,
                                long sub_id) {
  return ::ibetween(lo, hi, rgen, val_id, sub_id);
}

template <>
int64_t gen::between<int64_t>(int64_t lo, int64_t hi, long val_id,
                              long sub_id) {
  return ::ibetween(lo, hi, rgen, val_id, sub_id);
}

} // namespace runtime

const qqstdqqexception::control::mptr qqstdqqexception::control::mroulette[] =
    {};
qqstdqqexception::control::control(runtime::gen &g, unsigned ctr) : g(g) {}

} // namespace ramfuzz
