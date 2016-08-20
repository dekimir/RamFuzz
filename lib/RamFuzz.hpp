#pragma once

#include <iostream>
#include <string>

#include "clang/Tooling/Tooling.h"

/// Returns RamFuzz tests for code.  On failure, returns "fail".
std::string ramfuzz(const std::string &code);

/// Runs RamFuzz tool action, capturing output in out.  Returns the
/// result of tool.run().
int ramfuzz(clang::tooling::ClangTool &tool, std::ostream &out = std::cout);
