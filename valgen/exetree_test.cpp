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

/// Invokes n.dfs(), recording all edges visited, in order.
edge_vector dfs_result(node& n) {
  struct {
    void operator()(const edge& e) { visited.push_back(&e); }
    edge_vector visited;
  } h;
  n.dfs(h);
  return h.visited;
}

TEST(DFS, SingleNode) {
  node n;
  EXPECT_TRUE(dfs_result(n).empty());
}

TEST(DFS, SingleEdge) {
  node root;
  root.find_or_add_edge(321);
  auto& e1 = *root.cbegin();
  EXPECT_EQ(edge_vector{&e1}, dfs_result(root));
}

TEST(DFS, MultipleEdges) {
  node root;
  root.find_or_add_edge(1);
  root.find_or_add_edge(2);
  root.find_or_add_edge(3);
  auto it = root.cbegin();
  auto &e1 = *it++, &e2 = *it++, &e3 = *it;
  EXPECT_EQ((edge_vector{&e1, &e2, &e3}), dfs_result(root));
}

TEST(DFS, Deep) {
  node root;
  auto n1 = root.find_or_add_edge(1);
  auto n2 = n1->find_or_add_edge(2);
  auto n3 = n2->find_or_add_edge(3);
  EXPECT_EQ((edge_vector{n1->incoming_edge(), n2->incoming_edge(),
                         n3->incoming_edge()}),
            dfs_result(root));
}

TEST(DFS, Branch) {
  // root > n1 > n2 > n3
  //           > n4
  //      > n5
  node root;
  auto n1 = root.find_or_add_edge(1);
  auto n2 = n1->find_or_add_edge(2);
  auto n3 = n1->find_or_add_edge(3);
  auto n4 = n1->find_or_add_edge(4);
  auto n5 = root.find_or_add_edge(5);
  EXPECT_EQ((edge_vector{n1->incoming_edge(), n2->incoming_edge(),
                         n3->incoming_edge(), n4->incoming_edge(),
                         n5->incoming_edge()}),
            dfs_result(root));
}

}  // namespace
