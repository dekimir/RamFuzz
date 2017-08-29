// Copyright 2016-2017 The RamFuzz contributors. All rights reserved.
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

#include <string>

#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Expr.h"
#include "clang/AST/PrettyPrinter.h"
#include "llvm/Support/raw_ostream.h"

/// True iff C is visible outside all its parent contexts.
bool globally_visible(const clang::CXXRecordDecl *C);

namespace ramfuzz {

/// RamFuzz printing policy.
clang::PrintingPolicy RFPP();

/// Gets a name from NamedDecl where it exists.  Where it doesn't, generates a
/// unique placeholder name to be used for that NamedDecl.
class NameGetter {
public:
  /// Copies placeholder_prefix internally for later use by get().
  NameGetter(const std::string &placeholder_prefix)
      : placeholder_prefix(placeholder_prefix) {}

  /// Gets the NamedDecl's name, if it exists and is non-empty.  If not, returns
  /// a placeholder prefixed by the constructor's argument.  The placeholder is
  /// unique to this NamedDecl and permanently associated with it.
  llvm::StringRef get(const clang::NamedDecl *);

private:
  const std::string placeholder_prefix;
  std::map<const clang::NamedDecl *, std::string> placeholders;
  unsigned watermark = 0;
};

/// Keeps class details permanently, even after AST is deleted.  Has enough
/// information to allow various ways of referencing the class in generated
/// code.  Examples:
/// - a simple class A is referenced by just its name (if visible)
/// - a class in a namespace is referenced by its qualified name
/// - a class template is referenced by its name and template parameters, eg:
///   A<T1, T2>. But this requires a preamble like `template<class T1, class
///   T2>` somewhere before the reference.
class ClassDetails {
public:
  ClassDetails() = default;

  /// Parameters needn't survive past this constructor.
  explicit ClassDetails(const clang::CXXRecordDecl &, NameGetter &);

  /// The class's fully qualified name.  Meant to uniquely identify this object.
  const std::string &qname() const { return qname_; };

  /// Unqualified class name.
  const std::string &name() const { return name_; };

  /// Template preamble, eg, `template<typename T1, int n>`.  Empty if the class
  /// is not a template.
  const std::string &tpreamble() const { return prefix_; }

  /// Template parameters, eg, `<T1, n>`.  Empty if the class is not a template.
  const std::string &tparams() const { return suffix_; }

  bool operator<(const ClassDetails &that) const {
    return this->qname_ < that.qname_;
  }

  ClassDetails &operator=(const ClassDetails &that) = default;

  bool is_template() const { return is_template_; }

  bool is_visible() const { return is_visible_; }

private:
  std::string name_, qname_, prefix_, suffix_;
  bool is_template_; ///< True iff this is a class template.
  ///< True iff this class is visible from the outermost scope.
  bool is_visible_;
};

inline llvm::raw_ostream &operator<<(llvm::raw_ostream &os,
                                     const ClassDetails &cd) {
  return os << cd.qname() << cd.tparams();
}

/// Default template parameter name.
constexpr char default_typename[] = "ramfuzz_typename_placeholder";

} // namespace ramfuzz
