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

#include <gtest/gtest.h>
#include <vector>

#include "exetree.hpp"

using namespace std;
using namespace ramfuzz::exetree;

namespace {

using edge_vector = vector<const edge*>;

TEST(DFS, SingleNode) { EXPECT_FALSE(dfs_cursor(node())); }

TEST(DFS, SingleEdge) {
  node root;
  root.find_or_add_edge(321);
  auto cur = dfs_cursor(root);
  EXPECT_EQ(*root.cbegin(), *cur);
  EXPECT_TRUE(cur);
  EXPECT_FALSE(++cur);
}

TEST(DFS, MultipleEdges) {
  node root;
  root.find_or_add_edge(1.);
  root.find_or_add_edge(2.);
  root.find_or_add_edge(3.);
  auto it = root.cbegin();
  auto cur = dfs_cursor(root);
  EXPECT_TRUE(cur);
  EXPECT_EQ(3., *cur++);
  EXPECT_TRUE(cur);
  EXPECT_EQ(2., *cur++);
  EXPECT_TRUE(cur);
  EXPECT_EQ(1., *cur++);
  EXPECT_FALSE(cur);
}

TEST(DFS, Deep) {
  node root;
  root.find_or_add_edge(1.)->find_or_add_edge(2.)->find_or_add_edge(3.);
  auto cur = dfs_cursor(root);
  EXPECT_TRUE(cur);
  EXPECT_EQ(1., *cur++);
  EXPECT_TRUE(cur);
  EXPECT_EQ(2., *cur++);
  EXPECT_TRUE(cur);
  EXPECT_EQ(3., *cur++);
  EXPECT_FALSE(cur);
}

TEST(DFS, Branch) {
  // root > n1 > n2 > n3
  //           > n4
  //      > n5
  node root;
  root.find_or_add_edge(1.)->find_or_add_edge(2.)->find_or_add_edge(3.);
  root.find_or_add_edge(5.);
  root.cbegin()->dst()->find_or_add_edge(4.);
  auto cur = dfs_cursor(root);
  EXPECT_TRUE(cur);
  EXPECT_EQ(5., *cur++);
  EXPECT_TRUE(cur);
  EXPECT_EQ(1., *cur++);
  EXPECT_TRUE(cur);
  EXPECT_EQ(4., *cur++);
  EXPECT_TRUE(cur);
  EXPECT_EQ(2., *cur++);
  EXPECT_TRUE(cur);
  EXPECT_EQ(3., *cur++);
}

TEST(LongestPath, One) { EXPECT_EQ(1, longest_path(node())); }

TEST(LongestPath, Two) {
  node root;
  root.find_or_add_edge(1);
  EXPECT_EQ(2, longest_path(root));
  root.find_or_add_edge(2);
  EXPECT_EQ(2, longest_path(root));
  root.find_or_add_edge(3);
  EXPECT_EQ(2, longest_path(root));
}

TEST(LongestPath, Branch) {
  // root > n1 > n2 > n3
  //           > n4
  //      > n5
  node root;
  auto n1 = root.find_or_add_edge(1);
  auto n2 = n1->find_or_add_edge(2);
  auto n3 = n2->find_or_add_edge(3);
  auto n4 = n1->find_or_add_edge(4);
  auto n5 = root.find_or_add_edge(5);
  EXPECT_EQ(4, longest_path(root));
}

}  // namespace
