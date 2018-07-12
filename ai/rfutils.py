#!/usr/bin/env python

# Copyright 2016-2018 The RamFuzz contributors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""RamFuzz-related utilities.  Most depend on ../pymod being installed."""

import numpy as np
import ramfuzz
from functools import total_ordering


def logparse(f):
    """Parses a RamFuzz run log and yields each entry (a value/location pair) in
       turn."""
    fd = f.fileno()
    while True:
        entry = ramfuzz.load(fd)
        if entry is None:
            break
        yield entry


def loc2val(f):
    return {loc: val for (val, loc) in logparse(f)}


class indexes:
    """Assigns unique indexes to input values.

    An index is generated for each distinct value given to make_index().  In
    the object's lifetime, the same value always gets the same index.
    """

    def __init__(self):
        self.d = dict()
        self.watermark = 1

    def get_index(self, x):
        """Returns x's index, if it exists, otherwise None."""
        if x in self.d:
            return self.d[x]
        else:
            return None

    def make_index(self, x):
        """Like get_index, but makes a new index if it doesn't exist."""
        if x not in self.d:
            self.d[x] = self.watermark
            self.watermark += 1
        return self.d[x]


def count_locpos(files):
    """Counts distinct positions and locations in a list of files.

    Returns a pair (position count, location indexes object).
    """
    posmax = 0
    locidx = indexes()
    for fname in files:
        with open(fname) as f:
            for (pos, (val, loc)) in enumerate(logparse(f)):
                locidx.make_index(loc)
                posmax = max(posmax, pos)
    return posmax + 1, locidx


def read_data(files, poscount, locidx):
    """Builds input data from a files list."""
    locs = []  # One element per file; each is a list of location indexes.
    vals = []  # One element per file; each is a parallel list of values.
    labels = []  # One element per file: true for '.s', false for '.f'.
    for fname in files:
        flocs = np.zeros(poscount, np.uint64)
        fvals = np.zeros((poscount, 1), np.float64)
        with open(fname) as f:
            for (p, (v, l)) in enumerate(logparse(f)):
                idx = locidx.get_index(l)
                if idx:
                    flocs[p] = idx
                    fvals[p] = v
        locs.append(flocs)
        vals.append(fvals)
        labels.append(fname.endswith('.s'))
    return np.array(locs), np.array(vals), np.array(labels)


@total_ordering
class node(object):
    """A node in the execution-model tree.

    The execution-model tree represents observed behavior of the program under
    test, glimpsed from fuzzlogs of repeated program runs.  Each node in the
    tree is a location, while each edge is a value generated at the location of
    its origin node."""

    def __init__(self):
        self.loc = None
        """Location from fuzzlog."""
        self.edges = []
        """Each edge is a pair (value, node)."""
        self.terminal = False
        """If self is the end of execution, indicates its outcome.

        Either False, 'success' or 'failure'."""
        self.parent = None
        """Parent node."""

    @classmethod
    def from_literal(self, lit):
        """Creates a node from literal.

        The literal is a list of (value, location) pairs, just like the result
        of rfutils.logparse, but with the following twist: any element can be a
        dictionary or a string, rather than a pair.  When an element is a pair,
        it represents a node that's the sole descendant of the previous
        element and an outgoing edge with that element's value.  But when an
        element is a dictionary, it represents multiple edges from the previous
        element (which must be a pair), each leading to a different subtree.
        Each value in the dictionary is a literal representing the subtree;
        keys are ignored.  Each such literal must begin in the same location.

        Finally, when an element is a string, it must be either 'success' or
        'failure', and it represents a terminal node descending from the
        previous element."""

        class LiteralParseError(Exception):
            """An exception raised for erroneous literal."""

            def __init__(self, msg):
                self.msg = msg

            def __repr__(self):
                return 'LiteralParseError: ' + msg

        if not isinstance(lit, list):
            raise LiteralParseError('argument not a list')
        root = node()
        n = root
        for e in lit:
            if isinstance(e, tuple):
                if len(e) != 2:
                    raise LiteralParseError(
                        'tuples must be pairs')
                n = n.insert_or_descend(e[0], e[1])
            elif isinstance(e, dict):
                for sub in e.values():
                    n2 = node.from_literal(sub)
                    if n.loc is None:
                        n.loc = n2.loc
                    elif n.loc != n2.loc:
                        raise LiteralParseError(
                            'alternate paths must all begin in same loc')
                    n.edges.extend(n2.edges)
                for e in n.edges:
                    e[1].parent = n
            elif isinstance(e, str):
                if e not in ['success', 'failure']:
                    raise LiteralParseError(
                        'terminal must be "success" or "failure"')
                n.terminal = e

        return root

    def tostr(self, prefix=''):
        """Converts to a string, starting each new line with prefix."""
        if self.terminal:
            return prefix + self.terminal
        s = prefix + '%r' % self.loc
        indent = prefix + '  '
        for e in self.edges:
            s += '\n' + indent + '%r:\n' % e[0] + e[1].tostr(indent)
        return s

    def __repr__(self):
        return self.tostr()

    def __eq__(self, other):
        if self.terminal != other.terminal:
            return False
        if self.loc != other.loc:
            return False
        return sorted(self.edges) == sorted(other.edges)

    def __ne__(self, other):
        return not self == other

    def __lt__(self, other):
        if self.terminal < other.terminal:
            return True
        if self.loc < other.loc:
            return True
        return self.edges < other.edges

    class InconsistentBehavior(Exception):
        """Exception thrown when the execution model doesn't make sense."""

        def __init__(self, msg):
            self.msg = msg

        def __repr__(self):
            return 'InconsistentBehavior: ' + msg

    def insert_or_descend(self, val, loc):
        """Inserts a val edge from self to a new, empty node.

        Does nothing if such an edge already exists. In either case, returns
        the destination node of the edge."""
        if self.loc is None:
            self.loc = loc
        if self.loc != loc:
            raise node.InconsistentBehavior(
                '(%lf, %ld) when %ld expected' % (val, loc, self.loc))
        for v, n in self.edges:
            if v == val:
                return n
        newnode = node()
        newnode.parent = self
        self.edges.append((val, newnode))
        return newnode

    def add(self, log, successful):
        """Updates the tree rooted in self with another log.

        The log must reflect an execution of the same program."""
        curnode = self
        for v, l in log:
            curnode = curnode.insert_or_descend(v, l)
        term = 'success' if successful else 'failure'
        if not curnode.terminal:
            curnode.terminal = term
            if len(curnode.edges) > 0:
                raise node.InconsistentBehavior('Inner node marked terminal')
        elif curnode.terminal != term:
            raise node.InconsistentBehavior(
                '%s node marked %s' % (curnode.terminal, term))
