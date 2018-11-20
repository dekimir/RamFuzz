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
#include <set>

namespace ramfuzz {
namespace exetree {

struct node;

class edge {
 public:
  /// Creates a new node as a destination, owns its memory.
  edge(double value, node* src);
  operator double() const { return _value; }
  node* dst() const { return _dst.get(); }
  node* src() const { return _src; }

 private:
  double _value;
  node* _src;
  std::unique_ptr<node> _dst;
};

class node {
 public:
  enum TerminalStatus { INNER = 0, SUCCESS, FAILURE };
  node(edge* incoming_edge = nullptr)
      : has_valueid(false),
        _incoming_edge(incoming_edge),
        _terminal(INNER),
        _maywin(false) {}

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

  std::set<edge>::const_iterator cbegin() const { return edges.cbegin(); }
  std::set<edge>::const_iterator cend() const { return edges.cend(); }

  TerminalStatus terminal() const { return _terminal; }
  void terminal(TerminalStatus t) { _terminal = t; }

  bool maywin() const { return _maywin; }
  void maywin(bool mw) { _maywin = mw; }

  const edge* incoming_edge() const { return _incoming_edge; }

  /// Traverses n in depth-first-search preorder, invoking fn on each edge along
  /// the way.
  template <typename Callable>
  void dfs(Callable& fn) {
    for (auto& e : edges) {
      fn(e);
      e.dst()->dfs(fn);
    }
  }

 private:
  uint64_t valueid;
  bool has_valueid;
  edge* _incoming_edge;
  std::set<edge> edges;
  TerminalStatus _terminal;
  bool _maywin;  ///< True iff any descendant is SUCCESS.
};

/// The number of nodes in the longest path starting at root (ie, the tree
/// height).
size_t longest_path(const node& root);

}  // namespace exetree
}  // namespace ramfuzz
