#!/usr/bin/env python
"""Unit tests for the execution-model tree."""

from copy import deepcopy
from rfutils import node
import unittest


class TestEq(unittest.TestCase):
    def check_lit(self, a, b=None):
        """Asserts that nodes created from two literals are equal."""
        if b is None:
            b = a
        self.assertEqual(node.from_literal(a), node.from_literal(b))

    def test_empty(self):
        self.check_lit([])

    def test_seq1(self):
        self.check_lit([(1.0, 1L)])

    def test_seq2(self):
        self.check_lit([(1.0, 1L), (2.0, 2L)])

    def test_fork(self):
        self.check_lit([{1: [(1.0, 1L)], 2: [(2.0, 1L)]}])


class TestAdd(unittest.TestCase):
    def ck(self, lit, n):
        """Asserts that node n equals a literal."""
        self.assertEqual(node.from_literal(lit), n)

    def t(self, *logs):
        """Returns a tree with all logs added.

        Each logs element is a pair (log, string).  The string must be
        'success' or 'failure'."""
        n = node()
        for l in logs:
            n.add(l[0], l[1] == 'success')
        return n

    def test_identical(self):
        log = [(1.0, 1L), (2.0, 2L)]
        t = self.t((log, 'success'), (log, 'success'))
        self.ck([(1.0, 1L), (2.0, 2L), 'success'], t)

    def test_late_fork(self):
        log1 = [(1.0, 1L), (2.0, 2L), (3.0, 3L)]
        log2 = [(1.0, 1L), (2.0, 2L), (3.1, 3L)]
        t = self.t((log1, 'success'), (log2, 'success'))
        self.ck([(1.0, 1L), (2.0, 2L), {
            0: [(3.0, 3L), 'success'],
            1: [(3.1, 3L), 'success']}], t)

    def test_early_fork(self):
        log1 = [(1.0, 1L), (2.0, 2L), (3.0, 3L)]
        log2 = [(1.1, 1L), (2.0, 2L), (3.0, 3L)]
        t = self.t((log1, 'success'), (log2, 'failure'))
        self.ck([{
            0: [(1.0, 1L), (2.0, 2L), (3.0, 3L), 'success'],
            1: [(1.1, 1L), (2.0, 2L), (3.0, 3L), 'failure']}], t)

    def test_multi_fork(self):
        log1 = [(1.0, 1L), (2.1, 2L), (3.0, 3L)]
        log2 = [(1.0, 1L), (2.2, 2L), (3.0, 2L)]
        log3 = [(1.0, 1L), (2.2, 2L), (3.3, 2L), (4.0, 4L)]
        t = self.t((log1, 'success'), (log2, 'failure'), (log3, 'success'))
        self.ck([(1.0, 1L), {
            1: [(2.1, 2L), (3.0, 3L), 'success'],
            2: [(2.2, 2L), {
                2: [(3.0, 2L), 'failure'],
                3: [(3.3, 2L), (4.0, 4L), 'success']}]}], t)

    def test_inconsistent_loc(self):
        l1 = [(1.0, 10L), (2.0, 21L)]
        l2 = [(1.0, 10L), (2.0, 22L)]
        with self.assertRaises(node.InconsistentBehavior):
            t = self.t((l1, 'success'), (l2, 'success'))

    def test_inconsistent_outcome(self):
        l = [(1.0, 1L)]
        with self.assertRaises(node.InconsistentBehavior):
            t = self.t((l, 'success'), (l, 'failure'))

    def test_inconsistent_end(self):
        log1 = [(1.0, 1L), (2.0, 2L), (3.0, 3L)]
        log2 = log1[:2]
        with self.assertRaises(node.InconsistentBehavior):
            t = self.t((log1, 'success'), (log2, 'success'))


if __name__ == '__main__':
    unittest.main()
