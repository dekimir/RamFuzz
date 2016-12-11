// Copyright 2016 The RamFuzz contributors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <limits>
#include <ostream>
#include <random>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace ramfuzz {
namespace runtime {

/// Exception thrown when there's a file-access error.
struct file_error : public std::runtime_error {
  explicit file_error(const std::string &s) : runtime_error(s) {}
  explicit file_error(const char *s) : runtime_error(s) {}
};

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
/// in a separate file we call the index file.  The index file is a text file
/// where each line corresponds to a log region.  Each line begins with a number
/// that's the region ID, followed by a character '{', '}', or '|'.  The open
/// brace indicates the beginning of the region and is followed by a number
/// indicating the start position of that region in the log.  Conversely, the
/// closed brace indicates the end of the region and is followed by a number
/// indicating the start position of that region in the log.  Finally, the pipe
/// character indicates a single-value region and is followed by two numbers:
/// the first is the log position starting that value, and the second is the log
/// position right after the value.  Since each single value is independent,
/// they all have region ID of 0.  All position values are as returned by
/// std::ofstream::tellp().
///
/// During a replay, something needs to tell us _which_ region(s) to
/// skip/regenerate during replay; that something is the input-log control file.
/// See the skip class for that file's format.
class gen {
  /// Are we generating values or replaying a previous run?
  enum { generate, replay } runmode;

public:
  /// Values will be generated and logged in ologname (with index in
  /// ologname.i).
  gen(const std::string &ologname = "fuzzlog");

  /// Values will be replayed from ilogname (controlled by ilogname.c) and
  /// logged into ologname.
  gen(const std::string &ilogname, const std::string &ologname);

  /// Interprets kth command-line argument.  If the argument exists (ie, k <
  /// argc), values will be replayed from file named argv[k], controlled by
  /// argv[k]+"c", logged in argv[k]+"+", and indexed in argv[k]+"+.i".  If the
  /// argument doesn't exist, values will be generated, logged in "fuzzlog", and
  /// indexed in "fuzzlog.i".
  ///
  /// This makes it convenient for main(argc, argv) to invoke gen(argc, argv),
  /// yielding a program that either generates its values (if no command-line
  /// arguments) or replays the log file named by its first argument.
  gen(int argc, const char *const *argv, size_t k = 1);

  /// Returns an unconstrained random value of numeric type T, logs it, and
  /// indexes it.  When replaying the log, this value could be modified without
  /// affecting the replay of the rest of the log.
  template <typename T> T any() {
    return between(std::numeric_limits<T>::min(),
                   std::numeric_limits<T>::max());
  }

  /// Returns a random value of type T between lo and hi, inclusive, logs it,
  /// and indexes it.  When replaying the log, this value could be modified
  /// without affecting the replay of the rest of the log.
  template <typename T> T between(T lo, T hi) {
    olog_index << "0|" << olog.tellp();
    T val = gen_or_read(lo, hi);
    olog_index << ' ' << olog.tellp() << '\n';
    return val;
  }

  /// Sets obj to any().  Specialized in RamFuzz-generated code for classes
  /// under test.
  template <typename T> void set_any(T &obj) { obj = any<T>(); }

  /// Pointer overload of set_any().
  template <typename T> void set_any(T *&obj) { obj = new T(any<T>()); }

  /// Const pointer overload of set_any().
  template <typename T> void set_any(const T *&obj) { obj = new T(any<T>()); }

  /// void* overload of set_any().
  inline void set_any(void *&obj); // Defined below.

  /// Bool vector overload of set_any().
  void set_any(std::vector<bool>::reference obj);

  /// Assigns ctl.obj to obj if ctl is valid and obj is assignable.  Useful for
  /// implementing set_any for a class that has a RamFuzz control: you
  /// spin_roulette(), then feed the result to this method.
  template <typename RamFuzzControl,
            typename std::enable_if<
                std::is_copy_assignable<decltype(RamFuzzControl::obj)>::value,
                int>::type = 0>
  void assign(RamFuzzControl &ctl, decltype(RamFuzzControl::obj) obj) {
    if (ctl)
      obj = ctl.obj;
  }

  /// Overload of assign() for when obj isn't assignable.  Does nothing at
  /// runtime, but allows \c assign(ctl,obj) to always compile correctly,
  /// whether obj is assignable or not.
  ///
  /// One alternative to this arrangement is for the RamFuzz code generator to
  /// catch unassignable classes under test at code-generation time and only
  /// generate assign() for the assignable classes.  But determining whether
  /// copy assignment is implicitly deleted requires a great deal of expensive
  /// semantic analysis over more classes than RamFuzz generator is currently
  /// applied to (see, eg, how Sema::ShouldDeleteSpecialMember does it).
  /// Instead, we chose to generate assign() for all classes under test and use
  /// template magic to avoid compilation failures.  Quietly ignoring the
  /// unassignables isn't a problem, since this is intended for use by code
  /// supporting STL containers requiring assignable types anyway.
  template <typename RamFuzzControl,
            typename std::enable_if<
                !std::is_copy_assignable<decltype(RamFuzzControl::obj)>::value,
                int>::type = 0>
  void assign(RamFuzzControl &ctl, decltype(RamFuzzControl::obj) obj) {}

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
      return g.gen_or_read(lo, hi);
    }

  private:
    gen &g;
    uint64_t id;
  };

private:
  /// In generating mode, generates a random value between lo and hi.  In replay
  /// mode, reads the next value from the input log.  Either way, logs the value
  /// in the output log and returns it.
  template <typename T> T gen_or_read(T lo, T hi) {
    T val;
    if (runmode == generate)
      val = uniform_random(lo, hi);
    else {
      const auto pos = ilog.tellg();
      ilog.read(reinterpret_cast<char *>(&val), sizeof(val));
      // NB: pos now differs from ilog.tellg()!
      if (to_skip && pos >= to_skip.start() && pos < to_skip.end())
        val = uniform_random(lo, hi);
      if (to_skip && ilog.tellg() >= to_skip.end())
        to_skip = skip(ilog_ctl);
    }
    olog.write(reinterpret_cast<char *>(&val), sizeof(val));
    return val;
  }

  /// Returns a random value distributed uniformly between lo and hi, inclusive.
  /// Logs the value in olog.
  template <typename T> T uniform_random(T lo, T hi);

  /// Tracks which part of the input log to skip.
  class skip {
  public:
    skip() : valid(false) {}

    /// Reads the next skip line from str and initializes self from it.  The
    /// line must be of the format "start end", where "start" is the position in
    /// the replay log where the skip begins, and "end' is the position in the
    /// replay log where the replay resumes.  In other words, we skip over the
    /// region [start,end) from the replay log.  The start/end numbers are
    /// obtained from the log index -- see the index format description in the
    /// gen class blurb.
    skip(std::istream &str);

    operator bool() const { return valid; }
    bool region() const { return region_; }
    std::streampos start() const { return start_; }
    std::streampos end() const { return end_; }

  private:
    bool valid;   ///< True if type/start/end are meaningful.
    bool region_; ///< If true, skip whole region; otherwise skip single value.
    std::streamoff start_, end_; ///< Skip start/end position in the log.
  };

  /// Used for random value generation.
  std::ranlux24 rgen = std::ranlux24(std::random_device{}());

  /// Output log and its index.
  std::ofstream olog, olog_index;

  /// Region ID high water mark.
  uint64_t next_reg = 1;

  /// Input log (in replay mode) and the control file.
  std::ifstream ilog, ilog_ctl;

  /// If valid, holds the position of the next ilog part to skip.
  skip to_skip;
};

/// The upper limit on how many times to spin the method roulette in generated
/// RamFuzz classes.  Should be defined in user's code.
extern unsigned spinlimit;

/// Limit on the call-stack depth in generated RamFuzz methods.  Without such a
/// limit, infinite recursion is possible for certain code under test (eg,
/// ClassA::method1(B b) and ClassB::method2(A a)).  The user can modify this
/// value or the depthlimit member of any RamFuzz class.
constexpr unsigned depthlimit = 20;

/// Constructs a RamFuzz control instance by randomly spinning croulette.  Only
/// valid if croulette exists (ie, the class under test has public
/// constructors).
template <typename ramfuzz_control, int enable_if = ramfuzz_control::ccount>
ramfuzz_control construct(ramfuzz::runtime::gen &g) {
  return ramfuzz_control(g, g.between(0u, ramfuzz_control::ccount - 1));
}

/// Constructs a RamFuzz control instance by invoking control::make().  Only
/// valid if make() exists (ie, the class under test has no public
/// constructors).
template <typename ramfuzz_control,
          ramfuzz_control (*makefun)(runtime::gen &) = &ramfuzz_control::make>
ramfuzz_control construct(ramfuzz::runtime::gen &g) {
  return makefun(g);
}

/// Creates a RamFuzz control instance and randomly spins its mroulette.
template <typename ramfuzz_control>
ramfuzz_control spin_roulette(ramfuzz::runtime::gen &g) {
  gen::region reg(g);
  auto ctl = construct<ramfuzz_control>(g);
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

namespace rfstd_exception {
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
} // namespace rfstd_exception

namespace rfstd_vector {
template <typename Tp, typename Alloc = ::std::allocator<Tp>> class control {
private:
  // Declare first to initialize early; constructors may use it.
  runtime::gen &g;

public:
  std::vector<Tp, Alloc> obj;

  control(runtime::gen &g, unsigned) : g(g) {
    runtime::gen::region reg(g);
    obj.resize(reg.between(0u, 1000u));
    for (int i = 0; i < obj.size(); ++i)
      g.set_any(obj[i]);
  }

  Tp *release() {
    const auto sz = obj.size();
    Tp *cp = new Tp[sz];
    std::copy(obj.cbegin(), obj.cend(), cp);
    return cp;
  }

  operator bool() const { return true; }

  using mptr = void (control::*)();
  static constexpr unsigned mcount = 0;
  static const mptr mroulette[mcount];

  static constexpr unsigned ccount = 1;
};
} // namespace rfstd_vector

template <typename Tp, typename Alloc>
const typename rfstd_vector::control<Tp, Alloc>::mptr
    rfstd_vector::control<Tp, Alloc>::control::mroulette[] = {};

namespace rfstd_basic_string {
template <class CharT, class Traits, class Allocator> class control {
private:
  // Declare first to initialize early; constructors may use it.
  runtime::gen &g;

public:
  std::basic_string<CharT, Traits, Allocator> obj;
  control(runtime::gen &g, unsigned) : g(g) {
    runtime::gen::region reg(g);
    obj.resize(reg.between(1u, 1000u));
    for (int i = 0; i < obj.size() - 1; ++i)
      obj[i] = g.between<CharT>(1, std::numeric_limits<CharT>::max());
    obj.back() = CharT(0);
  }
  operator bool() const { return true; }
  std::basic_string<CharT, Traits, Allocator> *release() {
    return new std::basic_string<CharT, Traits, Allocator>(obj);
  }
  using mptr = void (control::*)();
  static constexpr unsigned mcount = 0;
  static const mptr mroulette[mcount];
  static constexpr unsigned ccount = 1;
};
} // namespace rfstd_basic_string

template <class CharT, class Traits, class Allocator>
const typename rfstd_basic_string::control<CharT, Traits, Allocator>::mptr
    rfstd_basic_string::control<CharT, Traits,
                                Allocator>::control::mroulette[] = {};

template <> void runtime::gen::set_any<std::string>(std::string &obj);

namespace rfstd_basic_istream {
template <class CharT, class Traits = std::char_traits<CharT>> class control {
  // Declare first to initialize early; constructors may use it.
  runtime::gen &g;
  rfstd_basic_string::control<CharT, Traits, std::allocator<CharT>> strctl;

public:
  std::basic_istringstream<CharT, Traits> obj;
  control(runtime::gen &g, unsigned) : g(g), strctl(g, 0), obj(strctl.obj) {}
  operator bool() const { return true; }
  using mptr = void (control::*)();
  static constexpr unsigned mcount = 0;
  static const mptr mroulette[mcount];
  static constexpr unsigned ccount = 1;
};
} // namespace rfstd_basic_istream

template <typename Tp, typename Alloc>
const typename rfstd_basic_istream::control<Tp, Alloc>::mptr
    rfstd_basic_istream::control<Tp, Alloc>::control::mroulette[] = {};

namespace rfstd_basic_ostream {
template <class CharT, class Traits = std::char_traits<CharT>> struct control {
  std::basic_ostringstream<CharT, Traits> obj;
  control(runtime::gen &g, unsigned) {}
  operator bool() const { return true; }
  using mptr = void (control::*)();
  static constexpr unsigned mcount = 0;
  static const mptr mroulette[mcount];
  static constexpr unsigned ccount = 1;
};
} // namespace rfstd_basic_ostream

template <typename Tp, typename Alloc>
const typename rfstd_basic_ostream::control<Tp, Alloc>::mptr
    rfstd_basic_ostream::control<Tp, Alloc>::control::mroulette[] = {};

namespace runtime {
// Must be defined after rfstd_vector.
inline void gen::set_any(void *&obj) {
  obj = rfstd_vector::control<char>(*this, 0).release();
}
} // namespace runtime

} // namespace ramfuzz
