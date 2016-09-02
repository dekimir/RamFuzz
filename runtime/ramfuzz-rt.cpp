// Copyright 2016 Heavy Automation Limited.  All rights reserved.

#include "ramfuzz-rt.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <iostream>
#include <limits>
#include <sstream>

using std::cout;
using std::endl;
using std::generate;
using std::hex;
using std::isprint;
using std::make_pair;
using std::noshowbase;
using std::numeric_limits;
using std::ostream;
using std::ostringstream;
using std::size_t;
using std::string;
using std::uniform_int_distribution;
using std::uniform_real_distribution;

namespace {
template <typename IntegralT>
IntegralT ibetween(IntegralT lo, IntegralT hi, std::ranlux24 &gen,
                   const string &val_id) {
  const auto val = uniform_int_distribution<IntegralT>{lo, hi}(gen);
  if (!val_id.empty())
    cout << val_id << "=" << val << endl;
  return val;
}

template <typename RealT>
RealT rbetween(RealT lo, RealT hi, std::ranlux24 &gen, const string &val_id) {
  const auto val = uniform_real_distribution<RealT>{lo, hi}(gen);
  if (!val_id.empty())
    cout << val_id << "=" << val << endl;
  return val;
}

/// Outputs str to os, escaping unprintable characters.
void stream_printable(ostream &os, const string &str) {
  const auto flags = os.flags();
  os << hex;
  for (char c : str)
    if (c == '\\')
      os << "\\\\";
    else if (isprint(c))
      os << c;
    else
      os << "\\x" << int{c / 16} << int{c % 16};
  os.flags(flags);
}
} // anonymous namespace

namespace ramfuzz {
namespace runtime {

template <> bool gen::any<bool>(const string &val_id) {
  return ::ibetween(0, 1, rgen, val_id);
}

template <> int gen::any<int>(const string &val_id) {
  return ::ibetween(numeric_limits<int>::min(), numeric_limits<int>::max(),
                    rgen, val_id);
}

template <> unsigned gen::any<unsigned>(const string &val_id) {
  return ::ibetween(numeric_limits<unsigned>::min(),
                    numeric_limits<unsigned>::max(), rgen, val_id);
}

template <> double gen::any<double>(const string &val_id) {
  return ::rbetween(numeric_limits<double>::min(),
                    numeric_limits<double>::max(), rgen, val_id);
}

template <> string gen::any<string>(const string &val_id) {
  static const int maxlen = 1025;
  const int len = ::ibetween(0, maxlen, rgen, "");
  char val[maxlen];
  generate(val, val + len, [this]() {
    return ::ibetween(char{1}, numeric_limits<char>::max(), rgen, "");
  });
  val[len] = 0;
  if (!val_id.empty()) {
    cout << val_id << "=";
    stream_printable(cout, val);
    cout << endl;
  }
  return val;
}

template <> int gen::between<int>(int lo, int hi, const string &val_id) {
  return ::ibetween(lo, hi, rgen, val_id);
}

template <>
size_t gen::between<size_t>(size_t lo, size_t hi, const string &val_id) {
  return ::ibetween(lo, hi, rgen, val_id);
}

template <>
unsigned gen::between<unsigned>(unsigned lo, unsigned hi,
                                const string &val_id) {
  return ::ibetween(lo, hi, rgen, val_id);
}

template <>
int64_t gen::between<int64_t>(int64_t lo, int64_t hi, const string &val_id) {
  return ::ibetween(lo, hi, rgen, val_id);
}

} // namespace runtime
} // namespace ramfuzz
