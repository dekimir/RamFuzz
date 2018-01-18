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
"""Greps the contents of one or more RamFuzz run logs.

Usage: $0 <location> [<filename> ...]

where <location> is a location number as reported by ./logdump.py.

"""

import rfutils
import sys

if len(sys.argv) < 2:
    print 'usage: %s <location> [<filename> ...]' % sys.argv[0]
    exit(1)

loc = long(sys.argv[1])
for fn in sys.argv[2:]:
    with open(fn) as f:
        for line, entry in enumerate(rfutils.logparse(f)):
            if entry[1] == loc:
                print '%s:%d %r' % (fn, line, entry)
