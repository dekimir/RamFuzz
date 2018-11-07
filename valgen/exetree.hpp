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

#pragma once

#include <memory>
#include <vector>

namespace ramfuzz {
namespace exetree {

struct node;

class edge {
 public:
  /// Creates a new node as a destination, owns its memory.
  edge(double value, node* src);
  operator double() const { return _value; }
  node* dst() const { return _dst.get(); }

 private:
  double _value;
  node* _src;
  std::unique_ptr<node> _dst;
};

class node {
 public:
  node(edge* incoming_edge = nullptr)
      : has_valueid(false), incoming_edge(incoming_edge) {}

  bool check_valueid(uint64_t expected) const {
    return !has_valueid || valueid == expected;
  }

  bool valueid_is(uint64_t v) const { return has_valueid && valueid == v; }

  void set_valueid(uint64_t v) {
    valueid = v;
    has_valueid = true;
  }

  /// Finds the outgoing edge matching v (or creates one, if none existed) and
  /// returns that edge's destination node.
  node* find_or_add_edge(double v);

  std::vector<edge>::const_iterator cbegin() const { return edges.cbegin(); }
  std::vector<edge>::const_iterator cend() const { return edges.cend(); }

 private:
  uint64_t valueid;
  bool has_valueid;
  edge* incoming_edge;
  std::vector<edge> edges;
};

}  // namespace exetree
}  // namespace ramfuzz
