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
IntegralT ibetween(IntegralT lo, IntegralT hi, ranlux24 &gen, ofstream &log) {
  auto val = uniform_int_distribution<IntegralT>{lo, hi}(gen);
  log.write(reinterpret_cast<char *>(&val), sizeof(val));
  return val;
}

template <typename RealT>
RealT rbetween(RealT lo, RealT hi, ranlux24 &gen, ofstream &log) {
  auto val = uniform_real_distribution<RealT>{lo, hi}(gen);
  log.write(reinterpret_cast<char *>(&val), sizeof(val));
  return val;
}

} // anonymous namespace

namespace ramfuzz {
namespace runtime {

template <> bool gen::gbetw<bool>(bool lo, bool hi) {
  return ::ibetween(lo, hi, rgen, olog);
}

template <> double gen::gbetw<double>(double lo, double hi) {
  return ::rbetween(lo, hi, rgen, olog);
}

template <> float gen::gbetw<float>(float lo, float hi) {
  return ::rbetween(lo, hi, rgen, olog);
}

void gen::set_any(std::vector<bool>::reference obj) { obj = any<bool>(); }

template <> int gen::gbetw<int>(int lo, int hi) {
  return ::ibetween(lo, hi, rgen, olog);
}

template <> size_t gen::gbetw<size_t>(size_t lo, size_t hi) {
  return ::ibetween(lo, hi, rgen, olog);
}

template <> unsigned gen::gbetw<unsigned>(unsigned lo, unsigned hi) {
  return ::ibetween(lo, hi, rgen, olog);
}

template <> int64_t gen::gbetw<int64_t>(int64_t lo, int64_t hi) {
  return ::ibetween(lo, hi, rgen, olog);
}

template <> char gen::gbetw<char>(char lo, char hi) {
  return ::ibetween(lo, hi, rgen, olog);
}

} // namespace runtime

const qqstdqqexception::control::mptr qqstdqqexception::control::mroulette[] =
    {};

qqstdqqexception::control::control(runtime::gen &g, unsigned ctr) : g(g) {}

} // namespace ramfuzz
