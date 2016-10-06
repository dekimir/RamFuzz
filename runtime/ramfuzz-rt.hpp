// Copyright 2016 Heavy Automation Limited.  All rights reserved.

#pragma once

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <limits>
#include <ostream>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace ramfuzz {
namespace runtime {

/// Generates values for RamFuzz code.  Can be used in the "generate" or
/// "replay" mode.  In "generate" mode, values are created at random and logged.
/// In "replay" mode, values are read from a previously generated log.  This
/// allows debugging of failed tests.
///
/// Additionally, it is possible in replay mode to depart from the log for a
/// while and then return to replaying it.  This allows fuzzing: mutate the
/// input in order to run a slightly different test, then keep the mutations
/// that (somehow) prove themselves useful.  The idea is to replace a continuous
/// part of the input log with another execution path dictated by freshly
/// generated values, skip that part of the log, and resume the replay from the
/// log point right after it.  To make this possible, RamFuzz code marks log
/// regions as it executes.  Each region is a continuous subset of the log that
/// can be replaced by a different execution path during replay without
/// affecting the replay of the rest of the log.  For example, a spin_roulette()
/// invocation is a region, as one set of spins can be wholly replaced by
/// another.
///
/// For clarity, the regions aren't marked in the output log itself but rather
/// in a separate file we call the index file.
///
/// During a replay, something needs to tell us _which_ region(s) to
/// skip/regenerate during replay, and that something is the input-log control
/// file.
class gen {
  /// Are we generating values or replaying already generated ones?
  enum { generate, replay } runmode;

public:
  /// Values will be generated and logged in ologname (with index in
  /// ologname.i).
  gen(const std::string &ologname = "fuzzlog")
      : runmode(generate), olog(ologname), olog_index(ologname + ".i") {}

  /// Values will be replayed from ilogname (controlled by ilogname.c) and
  /// logged into ologname.
  gen(const std::string &ilogname, const std::string &ologname)
      : runmode(replay), olog(ologname), olog_index(ologname + ".i"),
        ilog(ilogname), ilog_ctl(ilogname + ".c") {}

  /// Returns an unconstrained random value of type T and logs it.
  template <typename T> T any() {
    return between(std::numeric_limits<T>::min(),
                   std::numeric_limits<T>::max());
  }

  /// Returns a random value of type T between lo and hi, inclusive, and logs
  /// it via scalar_region().
  template <typename T> T between(T lo, T hi) {
    T val;
    if (runmode == generate)
      val = uniform_random(lo, hi);
    else
      ilog.read(reinterpret_cast<char *>(&val), sizeof(val));
    scalar_region(val);
    return val;
  }

  /// Sets obj to any().  Specialized in RamFuzz-generated code for classes
  /// under test.
  template <typename T> void set_any(T &obj) { obj = any<T>(); }

  /// Bool vector overload of set_any().
  void set_any(std::vector<bool>::reference obj);

  friend class region;
  /// RAII for region (a continuous subset of the log that can be replaced by a
  /// different execution path during replay without affecting the replay of the
  /// rest of the log).
  class region {
  public:
    /// Marks the start of a new region in the output log index.
    region(gen &g) : g(g), id(g.next_reg++) {
      g.olog_index << id << '{' << g.olog.tellp() << std::endl;
    }

    /// Marks the end of a new region in the output log index.
    ~region() { g.olog_index << id << '}' << g.olog.tellp() << std::endl; }

    /// Every region must have a unique id, so no copying.
    region(const region &) = delete;

    /// Returns a random value of type T between lo and hi, inclusive, and logs
    /// it in a way that ties it to this region.  On replay, the value cannot be
    /// modified without regenerating the whole region.
    template <typename T> T between(T lo, T hi) {
      T val;
      if (g.runmode == generate)
        val = g.uniform_random(lo, hi);
      else
        g.ilog.read(reinterpret_cast<char *>(&val), sizeof(val));
      g.olog.write(reinterpret_cast<char *>(&val), sizeof(val));
      return val;
    }

  private:
    gen &g;
    uint64_t id;
  };

  /// Logs val, marking it a single-value region in the log index.  Returns val.
  template <typename T> T scalar_region(T val) {
    olog.write(reinterpret_cast<char *>(&val), sizeof(val));
    olog_index << next_reg++ << '|' << olog.tellp() << std::endl;
    return val;
  }

private:
  /// Returns a random value distributed uniformly between lo and hi, inclusive.
  /// Logs the value in olog.
  template <typename T> T uniform_random(T lo, T hi);

  /// Used for random value generation.
  std::ranlux24 rgen = std::ranlux24(std::random_device{}());

  /// Output log and its index.
  std::ofstream olog, olog_index;

  /// Region high water mark.
  uint64_t next_reg = 0;

  /// Input log (in replay mode) and its index.
  std::ifstream ilog, ilog_ctl;
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
  gen::region reg(g);
  const auto ctr = g.between(0u, ramfuzz_control::ccount - 1);
  ramfuzz_control ctl(g, ctr);
  if (!ctl)
    return ctl;
  if (ramfuzz_control::mcount) {
    const auto mspins = reg.between(0u, ::ramfuzz::runtime::spinlimit);
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
  control(runtime::gen &g, unsigned) : g(g) {
    runtime::gen::region reg(g);
    obj.resize(reg.between(0u, 1000u));
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
