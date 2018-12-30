// Copyright 2016-2018 The RamFuzz contributors. All rights reserved.
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

/// Header file for the RamFuzz runtime, which is wholly contained in
/// ./ramfuzz-rt.cpp.  Start by reading ramfuzz::runtime::gen, which generates
/// random values by leveraging the RamFuzz executable's output.

#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <functional>
#include <limits>
#include <ostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>
#include <zmqpp/context.hpp>
#include <zmqpp/message.hpp>
#include <zmqpp/socket.hpp>

namespace ramfuzz {

/// RamFuzz harness for testing C objects.
///
/// The harness class contains a pointer to C as a public member named obj.
/// There is an interface for invoking obj's methods with random parameters, as
/// described below.  The harness creates obj but does not own it; the client
/// code does.
///
/// The harness class has one method for each public non-static method of C.  A
/// harness method, when invoked, generates random arguments and invokes the
/// corresponding method under test.  Harness methods take no arguments, as they
/// are self-contained and generate random values internally.  Their return type
/// is void (except for constructors, as described below).
///
/// Each of C's public constructors also gets a harness method.  These harness
/// methods allocate a new C and invoke the corresponding C constructor.  They
/// return a pointer to the constructed object.
///
/// The count of constructor harness methods is kept in a member named ccount.
/// There is also a member named croulette; it's an array of ccount method
/// pointers, one for each constructor method.  The harness class itself has a
/// constructor that constructs a C instance using a randomly chosen C
/// constructor.  This constructor takes a runtime::gen reference as a
/// parameter.
///
/// The count of non-constructor harness methods is kept in a member named
/// mcount.  There is also a member named mroulette; it's an array of mcount
/// method pointers, one for each non-constructor harness method.
///
/// A member named subcount contains the number of C's direct subclasses.  A
/// member named submakers is an array of subcount pointers to functions of type
/// C*(runtime::gen&).  Each direct subclass D has a submakers element that
/// creates a random D object and returns a pointer to it.
template <class C> class harness;

namespace runtime {

/// The upper limit on how many times to spin the method roulette in generated
/// RamFuzz classes.  Should be defined in user's code.
extern unsigned spinlimit;

/// Maps an arithmetic type to a wider type that can be put into zmqpp::message.
template <typename T> struct widetype;

/// Declares W to be T's widetype.
#define WIDETYPE(T, W)                                                         \
  template <> struct widetype<T> { using type = W; }

WIDETYPE(bool, int64_t);
WIDETYPE(char, int64_t);
WIDETYPE(unsigned char, int64_t);
WIDETYPE(short, int64_t);
WIDETYPE(unsigned short, uint64_t);
WIDETYPE(int, int64_t);
WIDETYPE(unsigned int, uint64_t);
WIDETYPE(long, int64_t);
WIDETYPE(unsigned long, uint64_t);
WIDETYPE(long long, int64_t);
WIDETYPE(unsigned long long, uint64_t);
WIDETYPE(float, double);
WIDETYPE(double, double);

#undef WIDETYPE

/// Unique tag value for every widetype used.
template <typename T> uint8_t typetag();
template <> inline uint8_t typetag<int64_t>() { return 1; }
template <> inline uint8_t typetag<uint64_t>() { return 2; }
template <> inline uint8_t typetag<double>() { return 3; }

constexpr char const *default_valgen_endpoint = "ipc:///tmp/ramfuzz-socket";

/// Generates values for RamFuzz code.  Can be used in the "generate" or
/// "replay" mode.  In "generate" mode, values are created at random and logged.
/// In "replay" mode, values are read from a previously generated log.  This
/// allows debugging of failed tests.
///
/// Depends on test code that RamFuzz generates (see ../main.cpp) -- in fact,
/// the generated fuzz.hpp file contains `#include "ramfuzz-rt.hpp"`, because
/// they'll always be used together.
///
/// It is recommended to use the same gen object for generating all parameters
/// in one test run.  That captures them all in the log file, so the test can be
/// easily replayed, and the log can be processed by AI tools in ../ai.  See
/// also the constructor gen(argc, argv, k) below.
///
/// The log is in binary format, to ensure replay precision.  Each log entry
/// contains the value generated and an ID for that value that the caller
/// provides.  These IDs are later used for analysis and learning how to make
/// the values valid.
class gen {
public:
  /// Connects to a valgen process at endpoint.
  gen(const std::string &endpoint = default_valgen_endpoint)
      : valgen_socket(ctx, zmqpp::socket_type::request) {
    valgen_socket.connect(endpoint);
  }

  /// Convenience for main().
  gen(int argc, char *argv[])
      : gen(argc > 1 ? argv[1] : default_valgen_endpoint) {}

  /// Returns an unconstrained value of type T and logs it.  The value is random
  /// in "generate" mode but read from the input log in "replay" mode.
  ///
  /// If allow_subclass is true, the result may be an object of T's subclass.
  template <typename T> T *make(size_t valueid, bool allow_subclass = false) {
    return makenew<T>(valueid, allow_subclass);
  }

  /// Handy name for invoking make<T>(or_subclass).
  static constexpr bool or_subclass = true;

  /// Returns a value of numeric type T between lo and hi, inclusive.
  ///
  /// The value is obtained from the valgen given to the constructor.
  template <typename T> T between(T lo, T hi, size_t valueid) {
    static constexpr uint8_t VALUE_REQUEST = 0;
    // Pack widened-type version of lo, hi into an outgoing message.
    using W = typename widetype<T>::type;
    zmqpp::message request(VALUE_REQUEST, uint64_t{valueid}, typetag<W>(),
                           W{lo}, W{hi});
    if (!valgen_socket.send(request))
      throw std::runtime_error("valgen_socket.send() returned false");
    zmqpp::message response;
    if (!valgen_socket.receive(response))
      throw std::runtime_error("valgen_socket.receive() returned false");
    if (response.get<uint8_t>(0) != 11)
      throw std::runtime_error("valgen returned error status");
    return static_cast<T>(response.get<W>(1));
  }

private:
  /// Logs val and id to olog.
  template <typename U> void output(U val, size_t id) {
    olog.put(typetag(val));
    olog.write(reinterpret_cast<char *>(&val), sizeof(val));
    olog.write(reinterpret_cast<char *>(&id), sizeof(id));
    olog.flush();
  }

  /// Reads val from ilog and advances ilog to the beginning of the next value.
  template <typename T> void input(T &val) {
    const char ty = ilog.get();
    assert(ty == typetag(val));
    ilog.read(reinterpret_cast<char *>(&val), sizeof(val));
    size_t id;
    ilog.read(reinterpret_cast<char *>(&id), sizeof(id));
  }

  /// Provides a static const member named `value` that's true iff T is a char*
  /// (modulo const/volatile).
  template <typename T> struct is_char_ptr {
    static const auto value =
        std::is_pointer<T>::value &&
        std::is_same<char, typename std::remove_cv<typename std::remove_pointer<
                               T>::type>::type>::value;
  };

  /// Like the public make(), but creates a brand new object and never returns
  /// previously created ones.
  ///
  /// There are several overloads for different kinds of T: arithmetic types,
  /// classes, pointers, etc.
  template <typename T>
  T *makenew(size_t valueid,
             typename std::enable_if<std::is_arithmetic<T>::value ||
                                         std::is_enum<T>::value,
                                     bool>::type allow_subclass = false) {
    return new T(between(std::numeric_limits<T>::min(),
                         std::numeric_limits<T>::max(), valueid));
  }

  template <typename T>
  T *makenew(size_t valueid,
             typename std::enable_if<std::is_class<T>::value ||
                                         std::is_union<T>::value,
                                     bool>::type allow_subclass = false) {
    if (harness<T>::subcount && allow_subclass &&
        between(0., 1., valueid) > 0.5) {
      return (*harness<T>::submakers[between(
          size_t{0}, harness<T>::subcount - 1, valueid)])(*this);
    } else {
      harness<T> h(*this);
      if (h.mcount) {
        for (auto i = 0u, e = between(0u, runtime::spinlimit, valueid); i < e;
             ++i)
          (h.*h.mroulette[between(0u, h.mcount - 1, valueid)])();
      }
      return h.obj;
    }
  }

  template <typename T>
  T *makenew(
      size_t valueid,
      typename std::enable_if<std::is_void<T>::value, bool>::type = false) {
    return new char[between(1, 4196, valueid)];
  }

  template <typename T>
  T *makenew(size_t valueid,
             typename std::enable_if<std::is_pointer<T>::value &&
                                         !is_char_ptr<T>::value,
                                     bool>::type allow_subclass = false) {
    using pointee = typename std::remove_pointer<T>::type;
    return new T(
        make<typename std::remove_cv<pointee>::type>(valueid, allow_subclass));
  }

  /// Most of the time, char* should be a null-terminated string, so it gets its
  /// own overload.
  template <typename T>
  T *makenew(size_t valueid,
             typename std::enable_if<is_char_ptr<T>::value, bool>::type
                 allow_subclass = false) {
    auto r = new char *;
    const auto sz = *make<size_t>(1000000 + valueid);
    *r = new char[sz + 1];
    (*r)[sz] = '\0';
    for (size_t i = 0; i < sz; ++i)
      (*r)[i] = between(std::numeric_limits<char>::min(),
                        std::numeric_limits<char>::max(), valueid);
    return const_cast<T *>(r);
  }

  template <typename T>
  T *makenew(size_t valueid,
             typename std::enable_if<std::is_function<T>::value, bool>::type
                 allow_subclass = false) {
    // TODO: implement.  Either capture \c this somehow to make() a value of the
    // return type; or select randomly one of existing functions in the program
    // that fit the signature.
    return 0;
  }

  /// Used for random value generation.
  std::ranlux24 rgen = std::ranlux24(std::random_device{}());

  /// Output log.
  std::ofstream olog;

  /// Input log in replay mode.
  std::ifstream ilog;

  zmqpp::context ctx;
  zmqpp::socket valgen_socket;
};

/// Limit on the call-stack depth in generated RamFuzz methods.  Without such a
/// limit, infinite recursion is possible for certain code under test (eg,
/// ClassA::method1(B b) and ClassB::method2(A a)).  The user can modify this
/// value or the depthlimit member of any RamFuzz class.
constexpr unsigned depthlimit = 4;

} // namespace runtime

template <> class harness<std::exception> {
public:
  std::exception *obj;
  harness(runtime::gen &) : obj(new std::exception) {}
  operator bool() const { return true; }
  using mptr = void (harness::*)();
  static constexpr unsigned mcount = 0;
  static constexpr mptr mroulette[] = {};
  static constexpr unsigned ccount = 1;
  static constexpr size_t subcount = 0;
  static constexpr std::exception *(*submakers[])(runtime::gen &) = {};
};

template <typename Tp, typename Alloc> class harness<std::vector<Tp, Alloc>> {
private:
  // Declare first to initialize early; constructors may use it.
  runtime::gen &g;
  using Vec = std::vector<Tp, Alloc>;

public:
  Vec *obj;

  harness(runtime::gen &g)
      : g(g), obj(new Vec(*g.make<typename Vec::size_type>(1))) {
    for (size_t i = 0; i < obj->size(); ++i)
      (*obj)[i] = *g.make<typename std::remove_cv<Tp>::type>(2);
  }

  operator bool() const { return true; }
  using mptr = void (harness::*)();
  static constexpr unsigned mcount = 0;
  static constexpr mptr mroulette[] = {};
  static constexpr unsigned ccount = 1;
  static constexpr size_t subcount = 0;
  static constexpr Vec *(*submakers[])(runtime::gen &) = {};
};

template <class CharT, class Traits, class Allocator>
class harness<std::basic_string<CharT, Traits, Allocator>> {
private:
  // Declare first to initialize early; constructors may use it.
  runtime::gen &g;

public:
  std::basic_string<CharT, Traits, Allocator> *obj;
  harness(runtime::gen &g)
      : g(g), obj(new std::basic_string<CharT, Traits, Allocator>(
                  *g.make<size_t>(3), CharT())) {
    for (size_t i = 0; i < obj->size() - 1; ++i)
      obj[i] = g.between<CharT>(1, std::numeric_limits<CharT>::max(), 4);
    obj->back() = CharT(0);
  }
  operator bool() const { return true; }
  using mptr = void (harness::*)();
  static constexpr unsigned mcount = 0;
  static constexpr mptr mroulette[] = {};
  static constexpr unsigned ccount = 1;
  static constexpr size_t subcount = 0;
  static constexpr std::basic_string<CharT, Traits, Allocator> *(*submakers[])(
      runtime::gen &) = {};
};

template <class CharT, class Traits>
class harness<std::basic_istream<CharT, Traits>> {
  // Declare first to initialize early; constructors may use it.
  runtime::gen &g;

public:
  std::basic_istringstream<CharT, Traits> *obj;
  harness(runtime::gen &g)
      : g(g), obj(new std::basic_istringstream<CharT, Traits>(
                  *g.make<std::string>(5))) {}
  operator bool() const { return true; }
  using mptr = void (harness::*)();
  static constexpr unsigned mcount = 0;
  static constexpr mptr mroulette[] = {};
  static constexpr unsigned ccount = 1;
  static constexpr size_t subcount = 0;
  static constexpr std::basic_istream<CharT, Traits> *(*submakers[])(
      runtime::gen &) = {};
};

template <class CharT, class Traits>
class harness<std::basic_ostream<CharT, Traits>> {
public:
  std::basic_ostringstream<CharT, Traits> *obj;
  harness(runtime::gen &g) : obj(new std::basic_ostringstream<CharT, Traits>) {}
  operator bool() const { return true; }
  using mptr = void (harness::*)();
  static constexpr unsigned mcount = 0;
  static constexpr mptr mroulette[] = {};
  static constexpr unsigned ccount = 1;
  static constexpr size_t subcount = 0;
  static constexpr std::basic_ostream<CharT, Traits> *(*submakers[])(
      runtime::gen &) = {};
};

template <typename Res, typename... Args>
class harness<std::function<Res(Args...)>> {
public:
  using user_class = std::function<Res(Args...)>;
  user_class *obj;
  harness(runtime::gen &g)
      : obj(new user_class([&g](Args...) { return *g.make<Res>(6); })) {}
  operator bool() const { return true; }
  using mptr = void (harness::*)();
  static constexpr unsigned mcount = 0;
  static constexpr mptr mroulette[] = {};
  static constexpr unsigned ccount = 1;
  static constexpr size_t subcount = 0;
  static constexpr user_class *(*submakers[])(runtime::gen &) = {};
};

} // namespace ramfuzz
