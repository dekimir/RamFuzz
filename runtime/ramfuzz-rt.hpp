// Copyright 2016 Heavy Automation Limited.  All rights reserved.

#pragma once

#include <cstdlib>
#include <exception>
#include <random>
#include <utility>
#include <vector>

namespace ramfuzz {
namespace runtime {

/// Generates random values for certain types.
class gen {
public:
  /// Returns an unconstrained random value of type T.  Logs the returned value,
  /// together with val_id and sub_id.  Their combination should be unique, so
  /// the callsite can be identified unambiguously.
  template <typename T> T any(long val_id, long sub_id = 0);

  /// Returns a random value of type T between lo and hi, inclusive.  Logs the
  /// returned value like any().
  template <typename T> T between(T lo, T hi, long val_id, long sub_id = 0);

  /// Sets obj to any(val_id, sub_id).  Specialized in RamFuzz-generated code
  /// for classes under test.
  template <typename T> void set_any(T &obj, long val_id, long sub_id = 0) {
    obj = any<T>(val_id, sub_id);
  }

  void set_any(std::vector<bool>::reference obj, long val_id, long sub_id = 0);

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
ramfuzz_control make_control(ramfuzz::runtime::gen &g, long val_id,
                             long sub_id = 0) {
  ramfuzz_control ctl(g, g.between(0u, ramfuzz_control::ccount - 1, val_id, 1));
  if (ctl && ramfuzz_control::mcount) {
    const auto mspins = g.between(0u, ::ramfuzz::runtime::spinlimit, val_id, 2);
    for (auto i = 0u; i < mspins; ++i)
      (ctl.*
       ctl.mroulette[g.between(0u, ramfuzz_control::control::mcount - 1, val_id,
                               i + 3)])();
  }
  return ctl;
}

} // namespace runtime

namespace qqstdqqexception {
class control {
private:
  // Declare first to initialize early; constructors may use it.
  runtime::gen &g;

public:
  ::std::exception obj;
  control(runtime::gen &g, unsigned);
  operator bool() const { return true; }

  using mptr = void (control::*)();
  static constexpr unsigned mcount = 0;
  static const mptr mroulette[mcount];

  static constexpr unsigned ccount = 1;
};
} // namespace qqstdqqexception

namespace qqstdqqvector {
template <typename Tp, typename Alloc = ::std::allocator<Tp>> class control {
private:
  // Declare first to initialize early; constructors may use it.
  runtime::gen &g;

public:
  ::std::vector<Tp, Alloc> obj;
  control(runtime::gen &g, unsigned) : g(g), obj(g.between(0u, 1000u, 10)) {
    for (int i = 0; i < obj.size(); ++i)
      g.set_any(obj[i], 10, i);
  }
  operator bool() const { return true; }

  using mptr = void (control::*)();
  static constexpr unsigned mcount = 0;
  static const mptr mroulette[mcount];

  static constexpr unsigned ccount = 1;
};
} // namespace qqstdqqvector

template <typename Tp, typename Alloc>
const typename qqstdqqvector::control<Tp, Alloc>::mptr
    qqstdqqvector::control<Tp, Alloc>::control::mroulette[] = {};

} // namespace ramfuzz
