#pragma once

#include <iostream>
#include <string>

#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Tooling/Tooling.h"

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

/// Returns RamFuzz tests for code.  On failure, returns "fail".
std::string ramfuzz(const std::string &code);

/// Runs RamFuzz tool action, capturing output in out.  Returns the
/// result of tool.run().
int ramfuzz(clang::tooling::ClangTool &tool, std::ostream &out = std::cout);
