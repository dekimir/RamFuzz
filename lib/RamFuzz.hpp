#pragma once

#include <iostream>

#include "clang/ASTMatchers/ASTMatchFinder.h"

/// Generates ramfuzz code.
class RamFuzz : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
  RamFuzz(std::ostream &out = std::cout);

  clang::ast_matchers::MatchFinder &getMatchFinder() { return MF; }

  void
  run(const clang::ast_matchers::MatchFinder::MatchResult &Result) override;

private:
  std::ostream &out;
  clang::ast_matchers::MatchFinder MF;
};
