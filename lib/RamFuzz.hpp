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

#include <iostream>
#include <string>
#include <vector>

#include "clang/Tooling/Tooling.h"
#include "llvm/Support/raw_ostream.h"

/// Returns RamFuzz tests for code.  On failure, returns "fail".
std::string ramfuzz(const std::string &code);

/// Runs RamFuzz action in a ClangTool.
///
/// @return the result of tool.run().
int ramfuzz(
    clang::tooling::ClangTool &tool, ///< Tool to run.
    const std::vector<std::string>
        &sources,            ///< Names of source files that tool will process.
    llvm::raw_ostream &outh, ///< Where to output generated declarations.
    llvm::raw_ostream &outc  ///< Where to output generated code.
    );
