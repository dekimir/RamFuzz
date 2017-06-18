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

#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/ADT/Twine.h"

namespace ramfuzz {

/// Maps a class to all subclasses that inherit from it directly.  Classes are
/// represented by their fully qualified names.
using Inheritance = llvm::StringMap<llvm::StringSet<>>;

/// Keeps certain details about (sub)classes (represented by their fully
/// qualified names), such as: is it a template, is it public/protected/private,
/// etc.
class ClassDetails {
public:
  void setIsTemplate(llvm::StringRef name, bool value = true) {
    is_template[name] = value;
  }

  bool isTemplate(llvm::StringRef name) const {
    return is_template.lookup(name);
  }

  void setAccessSpecifier(llvm::StringRef name,
                          clang::AccessSpecifier spec = clang::AS_public) {
    access_spec[name] = spec;
  }

  clang::AccessSpecifier getAccessSpecifier(llvm::StringRef name) const {
    return access_spec.lookup(name);
  }

private:
  llvm::StringSet<> is_template;
  llvm::StringMap<clang::AccessSpecifier> access_spec;
};

/// Builds up an Inheritance object by analyzing all non-anonymous classes in
/// some source code.  Can be used standalone via process() or within an
/// existing ClangTool via tackOnto().
class InheritanceBuilder
    : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
  InheritanceBuilder() = default;

  /// Adds to MF a matcher that will build inheritance (capturing *this).
  void tackOnto(clang::ast_matchers::MatchFinder &MF);

  /// Adds inheritance among classes in Code to *this.
  void process(const llvm::Twine &Code);

  /// Convenience constructor directly from Code.
  InheritanceBuilder(const llvm::Twine &Code) { process(Code); }

  /// Match callback.  Expects Result to have a CXXRecordDecl* binding for
  /// "class".
  void
  run(const clang::ast_matchers::MatchFinder::MatchResult &Result) override;

  const Inheritance &getInheritance() const { return inh; }
  const ClassDetails &getClassDetails() const { return cdetails; }

private:
  Inheritance inh;       ///< Inheritance result being built.
  ClassDetails cdetails; ///< Class details collected along the way.
};

} // namespace ramfuzz
