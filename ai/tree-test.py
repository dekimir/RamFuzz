#!/usr/bin/env python
"""Unit tests for the execution-model tree."""

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
        'success' or 'failure'.

        """
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
            1: [(3.1, 3L), 'success']
        }], t)

    def test_early_fork(self):
        log1 = [(1.0, 1L), (2.0, 2L), (3.0, 3L)]
        log2 = [(1.1, 1L), (2.0, 2L), (3.0, 3L)]
        t = self.t((log1, 'success'), (log2, 'failure'))
        self.ck([{
            0: [(1.0, 1L), (2.0, 2L), (3.0, 3L), 'success'],
            1: [(1.1, 1L), (2.0, 2L), (3.0, 3L), 'failure']
        }], t)

    def test_multi_fork(self):
        log1 = [(1.0, 1L), (2.1, 2L), (3.0, 3L)]
        log2 = [(1.0, 1L), (2.2, 2L), (3.0, 2L)]
        log3 = [(1.0, 1L), (2.2, 2L), (3.3, 2L), (4.0, 4L)]
        t = self.t((log1, 'success'), (log2, 'failure'), (log3, 'success'))
        self.ck([(1.0, 1L), {
            1: [(2.1, 2L), (3.0, 3L), 'success'],
            2: [(2.2, 2L), {
                2: [(3.0, 2L), 'failure'],
                3: [(3.3, 2L), (4.0, 4L), 'success']
            }]
        }], t)

    def test_inconsistent_loc(self):
        l1 = [(1.0, 10L), (2.0, 21L)]
        l2 = [(1.0, 10L), (2.0, 22L)]
        with self.assertRaises(node.InconsistentBehavior):
            self.t((l1, 'success'), (l2, 'success'))

    def test_inconsistent_outcome(self):
        lit = [(1.0, 1L)]
        with self.assertRaises(node.InconsistentBehavior):
            self.t((lit, 'success'), (lit, 'failure'))

    def test_inconsistent_end(self):
        log1 = [(1.0, 1L), (2.0, 2L), (3.0, 3L)]
        log2 = log1[:2]
        with self.assertRaises(node.InconsistentBehavior):
            self.t((log1, 'success'), (log2, 'success'))

    def test_parent1(self):
        n = self.t(([(1., 1L)], 'success'))
        child = n.edges[0][1]
        self.assertIs(child.parent, n)

    def test_parent2(self):
        n = self.t(([(1., 1L), (2., 2L)], 'success'))
        child = n.edges[0][1]
        self.assertIs(child.parent, n)
        grandchild = child.edges[0][1]
        self.assertIs(grandchild.parent, child)

    def test_parent_fork(self):
        n = self.t(([(1.1, 1L)], 'success'), ([(1.2, 1L)], 'success'))
        self.assertIs(n.edges[0][1].parent, n)
        self.assertIs(n.edges[1][1].parent, n)

    def test_parent_lit(self):
        n = node.from_literal([(1., 1L), {
            0: [(2.0, 2L), 'success'],
            1: [(2.1, 2L), 'failure']
        }])
        # 1->2->success
        #     ->failure
        c = n.edges[0][1]
        self.assertIs(c.parent, n)
        self.assertIs(c.edges[0][1].parent, c)
        self.assertIs(c.edges[1][1].parent, c)

    def test_reaches_success(self):
        n = node()
        self.assertFalse(n.reaches_success)
        n.add([(1., 1L)], False)
        f1 = n.edges[0][1]
        self.assertFalse(n.reaches_success)
        self.assertFalse(f1.reaches_success)
        n.add([(1.1, 1L), (2., 2L), (3., 3L)], False)
        c2 = n.edges[1][1]
        c3 = c2.edges[0][1]
        f2 = c3.edges[0][1]
        self.assertFalse(n.reaches_success)
        self.assertFalse(f1.reaches_success)
        self.assertFalse(c2.reaches_success)
        self.assertFalse(c3.reaches_success)
        self.assertFalse(f2.reaches_success)
        n.add([(1.1, 1L), (2.1, 2L)], True)
        s1 = c2.edges[1][1]
        self.assertTrue(n.reaches_success)
        self.assertFalse(f1.reaches_success)
        self.assertTrue(c2.reaches_success)
        self.assertFalse(c3.reaches_success)
        self.assertFalse(f2.reaches_success)
        self.assertTrue(s1.reaches_success)

    def test_reaches_success_lityes(self):
        n = node.from_literal([{
            0: [(1.0, 1L), (2., 2L), 'failure'],
            1: [(1.1, 1L), (3., 3L), 'success']
        }])
        c2 = n.edges[0][1]
        c3 = n.edges[1][1]
        f = c2.edges[0][1]
        s = c3.edges[0][1]
        self.assertTrue(n.reaches_success)
        self.assertFalse(c2.reaches_success)
        self.assertFalse(f.reaches_success)
        self.assertTrue(c3.reaches_success)
        self.assertTrue(s.reaches_success)

    def test_reaches_success_litno(self):
        n = node.from_literal([{
            0: [(1.0, 1L), (2., 2L), 'failure'],
            1: [(1.1, 1L), (3., 3L), 'failure']
        }])
        c2 = n.edges[0][1]
        c3 = n.edges[1][1]
        f2 = c3.edges[0][1]
        self.assertFalse(n.reaches_success)
        self.assertFalse(c2.reaches_success)
        self.assertFalse(f2.reaches_success)
        self.assertFalse(c3.reaches_success)
        self.assertFalse(f2.reaches_success)


class TestRootPath(unittest.TestCase):
    def test_single_path(self):
        n0 = node.from_literal([(0., 0L), (1., 1L), (2., 2L)])
        n1 = n0.edges[0][1]
        n2 = n1.edges[0][1]
        self.assertEqual(n1.rootpath(), [n0, n1])
        self.assertEqual(n2.rootpath(), [n0, n1, n2])

    def test_fork(self):
        n0 = node.from_literal([(0., 0L), {
            0: [(1.0, 1L), (2., 2L)],
            1: [(1.1, 1L), (3., 3L)]
        }])
        n1 = n0.edges[0][1]
        n2 = n1.edges[0][1]
        n3 = n1.edges[1][1]
        self.assertEqual(n0.loc, 0L)
        self.assertEqual(n1.loc, 1L)
        self.assertEqual(n2.loc, 2L)
        self.assertEqual(n3.loc, 3L)
        self.assertEqual(n1.rootpath(), [n0, n1])
        self.assertEqual(n2.rootpath(), [n0, n1, n2])
        self.assertEqual(n3.rootpath(), [n0, n1, n3])

    def test_root(self):
        n = node()
        self.assertEqual(n.rootpath(), [n])


class TestLogSeq(unittest.TestCase):
    def test_root(self):
        self.assertEqual(node().logseq(), [])

    def test_single_path(self):
        n0 = node.from_literal([(0., 0L), (1., 1L), (2., 2L)])
        self.assertEqual(n0.edges[0][1].logseq(), [(0., 0L)])
        self.assertEqual(n0.edges[0][1].edges[0][1].logseq(), [(0., 0L),
                                                               (1., 1L)])
        self.assertEqual(n0.edges[0][1].edges[0][1].edges[0][1].logseq(),
                         [(0., 0L), (1., 1L), (2., 2L)])

    def test_fork(self):
        n0 = node.from_literal([(0., 0L), {
            0: [(1.0, 1L), (2., 2L)],
            1: [(1.1, 1L), (3., 3L)]
        }])
        n1 = n0.edges[0][1]
        n2 = n1.edges[0][1]
        n3 = n1.edges[1][1]
        self.assertEqual(n1.logseq(), [(0., 0L)])
        self.assertEqual(n2.logseq(), [(0., 0L), (1.0, 1L)])
        self.assertEqual(n3.logseq(), [(0., 0L), (1.1, 1L)])


class TestRepr(unittest.TestCase):
    def c(self, lit, rep):
        self.assertEqual(node.from_literal([lit]).__repr__(), rep)

    def test_success(self):
        self.c('success', 'success')

    def test_failure(self):
        self.c('failure', 'failure')

    def test_nonterm(self):
        self.c((1., 123L), '123L')

    def test_none(self):
        self.assertEqual(node().__repr__(), 'None')


class TestLocidx(unittest.TestCase):
    def c(self, lit, loclist):
        idx = node.from_literal(lit).locidx()
        for loc in loclist:
            self.assertIsNotNone(idx.get_index(loc))

    def test_empty(self):
        self.c(['success'], [])

    def test_single(self):
        self.c([(1.23, 123L)], [123L])

    def test_linear(self):
        self.c([(float(i), long(i)) for i in range(15)], range(15))

    def test_linear_single(self):
        self.c([(1., 1L) for i in range(99)], [1L])

    def test_fork(self):
        self.c([(1., 1L), {
            3: [(2., 2L), (3., 3L)],
            45: [(2., 2L), (4., 4L), (5., 5L)],
            678: [(2., 2L), (6., 6L), (7., 7L), (8., 8L)]
        }], range(1, 8))


class TestDepth(unittest.TestCase):
    def c(self, depth, lit):
        self.assertEqual(depth, node.from_literal(lit).depth())

    def test_single(self):
        self.c(1, [])

    def test_linear(self):
        self.c(4, [(1., 1L), (2., 2L), (3., 3L), 'failure'])

    def test_fork(self):
        self.c(3, [{
            0: [(1.0, 1L), 'success'],
            1: [(1.1, 1L), 'failure'],
            2: [(1.2, 1L), (2., 2L), 'success']
        }])


if __name__ == '__main__':
    unittest.main()
