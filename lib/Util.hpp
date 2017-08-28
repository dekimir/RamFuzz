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

/// Returns decl's name, if nonempty; otherwise, returns deflt.
llvm::StringRef getName(const clang::NamedDecl &decl, const char *deflt);

namespace ramfuzz {

/// RamFuzz printing policy.
clang::PrintingPolicy RFPP();

/// Keeps class details permanently, even after AST is deleted.
class ClassDetails {
public:
  ClassDetails() = default;
  /// CXXRecordDecl object needn't survive past this constructor.
  explicit ClassDetails(const clang::CXXRecordDecl &);
  const std::string &prefix() const { return prefix_; }
  const std::string &qname() const { return qname_; };
  const std::string &suffix() const { return suffix_; }
  bool operator<(const ClassDetails &that) const {
    return this->qname_ < that.qname_;
  }
  bool operator<(const std::string &s) const { return qname_ < s; }
  ClassDetails &operator=(const ClassDetails &that) = default;
  bool is_template() const { return is_template_; }
  bool is_visible() const { return is_visible_; }

private:
  std::string qname_, prefix_, suffix_;
  bool is_template_; ///< True iff this is a class template.
  ///< True iff this class is visible from the outermost scope.
  bool is_visible_;
};

inline bool operator<(const std::string &s, const ClassDetails &ref) {
  return ref.qname() < s;
}

inline llvm::raw_ostream &operator<<(llvm::raw_ostream &os,
                                     const ClassDetails &cd) {
  return os << cd.qname() << cd.suffix();
}

/// Default template parameter name.
constexpr char default_typename[] = "ramfuzz_typename_placeholder";

} // namespace ramfuzz
