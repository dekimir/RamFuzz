// Copyright 2016 Heavy Automation Limited.  All rights reserved.

#pragma once

#include <cstdlib>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace ramfuzz {
namespace runtime {

/// Generates random values for certain types.
class gen {
public:
  /// Returns an unconstrained random value of type T.  If val_id is non-empty,
  /// prints "<val_id>=<value>" on stdout.
  template <typename T> T any(const std::string &val_id = "");

  /// Returns a random value of type T between lo and hi, inclusive.  If val_id
  /// is non-empty, prints "<val_id>=<value>" on stdout.
  template <typename T> T between(T lo, T hi, const std::string &val_id = "");

private:
  /// Used for random value generation.
  std::ranlux24 rgen = std::ranlux24(std::random_device{}());
};

/// How many times to spin the method roulette in generated RamFuzz classes.
/// Should be defined in user's code.
extern unsigned spinlimit;

/// Limit on the call-stack depth in generated RamFuzz methods.  Without such a
/// limit, infinite recursion is possible for certain code under test (eg,
/// ClassA::method1(B b) and ClassB::method2(A a)).  The user can modify this
/// value or the depthlimit member of any RamFuzz class.
constexpr unsigned depthlimit = 20;

/// Returns an instance of a RamFuzz control after randomly spinning its
/// roulettes.
template <typename ramfuzz_control>
ramfuzz_control make_control(ramfuzz::runtime::gen &g) {
  ramfuzz_control ctl(
      g, g.between(0u, ramfuzz_control::ccount - 1, "make_control ctr"));
  if (ctl && ramfuzz_control::mcount) {
    const auto mspins = g.between(0u, ::ramfuzz::runtime::spinlimit, "mspins");
    for (auto i = 0u; i < mspins; ++i)
      (ctl.*
       ctl.mroulette[g.between(0u, ramfuzz_control::control::mcount - 1,
                               "spin")])();
  }
  return ctl;
}

} // namespace runtime
} // namespace ramfuzz
