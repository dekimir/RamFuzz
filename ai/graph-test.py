#!/usr/bin/env python
"""Unit tests for the execution-model graph."""

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


if __name__ == '__main__':
    unittest.main()
