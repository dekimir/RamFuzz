// Copyright 2016 Heavy Automation Limited.  All rights reserved.

#pragma once

#include <cstdlib>
#include <exception>
#include <fstream>
#include <limits>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace ramfuzz {
namespace runtime {

/// Generates values for RamFuzz code.  Can be used in the "generate" or
/// "replay" mode.  In "generate" mode, values are created at random and logged.
/// In "replay" mode, values are read from a previously generated log.
class gen {
  /// Are we generating values or replaying already generated ones?
  enum { generate, replay } runmode;

public:
  /// Values will be generated and logged in ologname.
  gen(const std::string &ologname = "fuzzlog")
      : runmode(generate), olog(ologname) {}

  /// Values will be replayed from ilogname and logged into ologname.
  gen(const std::string &ilogname, const std::string &ologname)
      : runmode(replay), olog(ologname), ilog(ilogname) {}

  /// Returns an unconstrained random value of type T and logs it.
  template <typename T> T any() {
    return between(std::numeric_limits<T>::min(),
                   std::numeric_limits<T>::max());
  }

  /// Returns a random value of type T between lo and hi, inclusive, and logs
  /// it.
  template <typename T> T between(T lo, T hi) {
    if (runmode == generate)
      return uniform_random(lo, hi);
    else {
      T val;
      ilog.read(reinterpret_cast<char *>(&val), sizeof(val));
      olog.write(reinterpret_cast<char *>(&val), sizeof(val));
      return val;
    }
  }

  /// Sets obj to any().  Specialized in RamFuzz-generated code for classes
  /// under test.
  template <typename T> void set_any(T &obj) { obj = any<T>(); }

  void set_any(std::vector<bool>::reference obj);

private:
  /// Returns a random value distributed uniformly between lo and hi, inclusive.
  /// Logs the value in olog.
  template <typename T> T uniform_random(T lo, T hi);

  /// Used for random value generation.
  std::ranlux24 rgen = std::ranlux24(std::random_device{}());

  /// Output log.
  std::ofstream olog;

  /// Input log (in replay mode).
  std::ifstream ilog;
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
  const auto ctr = g.between(0u, ramfuzz_control::ccount - 1);
  ramfuzz_control ctl(g, ctr);
  if (!ctl)
    return ctl;
  if (ramfuzz_control::mcount) {
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
