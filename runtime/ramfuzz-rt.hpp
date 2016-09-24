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
  /// Returns an unconstrained random value of type T.
  template <typename T> T any();

  /// Returns a random value of type T between lo and hi, inclusive.  Logs the
  /// returned value like any().
  template <typename T> T between(T lo, T hi);

  /// Sets obj to any().  Specialized in RamFuzz-generated code for classes
  /// under test.
  template <typename T> void set_any(T &obj) { obj = any<T>(); }

  void set_any(std::vector<bool>::reference obj);

private:
  /// Used for random value generation.
  std::ranlux24 rgen = std::ranlux24(std::random_device{}());
};

/// The upper limit on how many times to spin the method roulette in generated
/// RamFuzz classes.  Should be defined in user's code.
extern unsigned spinlimit;

/// Limit on the call-stack depth in generated RamFuzz methods.  Without such a
/// limit, infinite recursion is possible for certain code under test (eg,
/// ClassA::method1(B b) and ClassB::method2(A a)).  The user can modify this
/// value or the depthlimit member of any RamFuzz class.
constexpr unsigned depthlimit = 20;

/// Creates a RamFuzz control instance using a random spin of croulette,
/// followed by a random number of random spins of mroulette, if mroulette is
/// not empty.
template <typename ramfuzz_control>
ramfuzz_control spin_roulette(ramfuzz::runtime::gen &g) {
  ramfuzz_control ctl(g, g.between(0u, ramfuzz_control::ccount - 1));
  if (ctl && ramfuzz_control::mcount) {
    const auto mspins = g.between(0u, ::ramfuzz::runtime::spinlimit);
    for (auto i = 0u; i < mspins; ++i)
      (ctl.*
       ctl.mroulette[g.between(0u, ramfuzz_control::control::mcount - 1)])();
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
  control(runtime::gen &g, unsigned) : g(g), obj(g.between(0u, 1000u)) {
    for (int i = 0; i < obj.size(); ++i)
      g.set_any(obj[i]);
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
