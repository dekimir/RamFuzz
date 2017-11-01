#!/usr/bin/env python

# Copyright 2016-17 The RamFuzz contributors. All rights reserved.
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


def logparse(f):
    """Parses a RamFuzz run log and yields each entry (a value/location pair) in
       turn."""
    fd = f.fileno()
    while True:
        entry = ramfuzz.load(fd)
        if entry is None:
            break
        yield entry


class indexes:
    """Assigns unique indexes to input values.

    An index is generated for each distinct value given to get().  In the
    object's lifetime, the same value always gets the same index.
    """

    def __init__(self):
        self.d = dict()
        self.watermark = 1

    def get(self, x):
        if x not in self.d:
            self.d[x] = self.watermark
            self.watermark += 1
        return self.d[x]


def count_locpos(files):
    """Counts distinct positions and locations in a list of files.

    Returns a pair (position count, location indexes object)self.
    """
    posmax = 0
    locidx = indexes()
    for fname in files:
        with open(fname) as f:
            for (pos, (val, loc)) in enumerate(logparse(f)):
                locidx.get(loc)
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
                flocs[p] = locidx.get(l)
                fvals[p] = v
        locs.append(flocs)
        vals.append(fvals)
        labels.append(fname.endswith('.s'))
    return np.array(locs), np.array(vals), np.array(labels)
