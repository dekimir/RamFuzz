#!/usr/bin/python

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
"""Generates a RamFuzz training corpus.

Usage: $0 <executable> <count>

Runs <executable> and assumes it creates fuzzlog-ai.  If the executable's exit
status is 0, renames fuzzlog-ai to 1.s.  Otherwise, renames fuzzlog-ai to 1.f.
Repeats <count> times, incrementing the number in the .s/.f file name.

After <count> runs, this leaves a set of .s (for success) and .f (for failure)
files in the current directory.  This represents a corpus on which to train an
AI that divines how ramfuzz::runtime::gen should generate values in order to
avoid failures.
"""

import os
import subprocess
import sys

argc = len(sys.argv)
if argc != 3:
    sys.exit('usage: %s <executable> <count>' % sys.argv[0])

succ = 0
fail = 0
for _ in xrange(int(sys.argv[2])):
    if (subprocess.call(sys.argv[1]) == 0):
        os.rename('fuzzlog-ai', '%d.s' % succ)
        succ += 1
    else:
        os.rename('fuzzlog-ai', '%d.f' % fail)
        fail += 1
