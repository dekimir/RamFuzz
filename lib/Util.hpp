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

/// Prints \p params to \p os, together with their types.  Eg: "typename T1,
/// class T2, int T3".
void print_names_with_types(const clang::TemplateParameterList &params,
                            llvm::raw_ostream &os);

/// Returns decl's name, if nonempty; otherwise, returns deflt.
llvm::StringRef getName(const clang::NamedDecl &decl, const char *deflt);

namespace ramfuzz {

/// RamFuzz printing policy.
clang::PrintingPolicy RFPP();

/// Keeps class details permanently, even after AST is destructed.
class ClassDetails {
public:
  ClassDetails() = default;
  /// CXXRecordDecl object needn't survive past this constructor.
  explicit ClassDetails(const clang::CXXRecordDecl &);
  const std::string &prefix() const { return prefix_; }
  const std::string &name() const { return name_; };
  const std::string &suffix() const { return suffix_; }
  bool operator<(const ClassDetails &that) const {
    return this->name_ < that.name_;
  }
  bool operator<(const std::string &s) const { return name_ < s; }
  ClassDetails &operator=(const ClassDetails &that) = default;
  bool is_template() const { return is_template_; }
  bool is_visible() const { return is_visible_; }

private:
  std::string name_, prefix_, suffix_;
  bool is_template_; ///< True iff this is a class template.
  ///< True iff this class is visible from the outermost scope.
  bool is_visible_;
};

inline bool operator<(const std::string &s, const ClassDetails &ref) {
  return ref.name() < s;
}

inline llvm::raw_ostream &operator<<(llvm::raw_ostream &os,
                                     const ClassDetails &cd) {
  return os << cd.name() << cd.suffix();
}

/// Streams the preamble "template<...>" required before a template class's
/// name.  If the class isn't a template, streams nothing.
class template_preamble {
public:
  /// \p templ may be null, in which case \c print() prints nothing.
  template_preamble(const clang::ClassTemplateDecl *templ) : templ(templ) {}

  void print(llvm::raw_ostream &os) const {
    if (templ) {
      os << "template<";
      print_names_with_types(*templ->getTemplateParameters(), os);
      os << ">\n";
    }
  }

  std::string str() const {
    std::string stemp;
    llvm::raw_string_ostream rs(stemp);
    print(rs);
    return rs.str();
  }

private:
  const clang::ClassTemplateDecl *templ;
};

inline llvm::raw_ostream &operator<<(llvm::raw_ostream &os,
                                     const template_preamble &pream) {
  pream.print(os);
  return os;
}

/// Default template parameter name.
constexpr char default_typename[] = "ramfuzz_typename_placeholder";

} // namespace ramfuzz
