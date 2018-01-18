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

/// Tests generation, logging, and replay of all integer types.

#include <tuple>
#include <vector>

using std::get;
using std::vector;

struct A {
  std::tuple<vector<short>, vector<unsigned short>, vector<int>,
             vector<unsigned>, vector<long>, vector<unsigned long>,
             vector<long long>, vector<unsigned long long>>
      v;
  void f(short i) { get<0>(v).push_back(i); }
  void f(unsigned short i) { get<1>(v).push_back(i); }
  void f(int i) { get<2>(v).push_back(i); }
  void f(unsigned i) { get<3>(v).push_back(i); }
  void f(long i) { get<4>(v).push_back(i); }
  void f(unsigned long i) { get<5>(v).push_back(i); }
  void f(long long i) { get<6>(v).push_back(i); }
  void f(unsigned long long i) { get<7>(v).push_back(i); }
  bool operator!=(const A &that) { return v != that.v; }
};
