#pragma once

#include <iostream>

#include "clang/ASTMatchers/ASTMatchFinder.h"

/// Generates ramfuzz code.
class RamFuzz : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
  RamFuzz(std::ostream &out = std::cout);

  /// Creates a MatchFinder that will generate ramfuzz code when
  /// applied to the AST of code under test.  It marries *this action
  /// with a suitable AST matcher.
  clang::ast_matchers::MatchFinder makeMatchFinder();

  void
  run(const clang::ast_matchers::MatchFinder::MatchResult &Result) override;

private:
  std::ostream &out;
};
