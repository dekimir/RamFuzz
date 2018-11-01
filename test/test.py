#!/usr/bin/python

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
"""Runs the RamFuzz test battery.

Usage: $0 <llvm-build-dir>

where <llvm-build-dir> is the directory in which LLVM with RamFuzz was
built; this directory must contain bin/ramfuzz and bin/clang++ in it.

Each .hpp/.cpp file pair in this script's directory represents a test
case.  Each case will be run as follows:

1. Make a temporary directory and copy the .hpp and .cpp testcase
   files into it; also copy the requisite RamFuzz runtime there.

2. Run bin/ramfuzz on the .hpp file in the temporary directory,
   generating fuzz.hpp and fuzz.cpp.

3. Compile the .cpp file (which must #include fuzz.hpp) using
   bin/clang++ (also adding fuzz.cpp and runtime to produce an
   executable).

4. Run the compiled executable and treat its exit status as indication
   of success or failure.

5. On success, remove the temporary directory.

If any of the steps fail, the temporary directory is kept and a brief
error message is printed including the test name and the path to the
temporary directory.

A test case is usually structured as follows: the .hpp file contains
declarations that RamFuzz will process into fuzzing code, while the
.cpp file contains a main() function that exercises that fuzzing code
to verify its correctness.  For example, the main() function may want
to ensure that all methods under test have been called in some order.

"""

from glob import glob
from os import chdir, path
from subprocess import CalledProcessError, check_call, Popen
import shutil
import sys
import tempfile

if len(sys.argv) != 2:
    sys.exit('usage: %s <llvm-build-dir>' % sys.argv[0])
bindir = path.join(sys.argv[1], 'bin')
scriptdir = path.dirname(path.realpath(__file__))
rtdir = path.join(scriptdir, '..', 'runtime')
failures = 0
for case in glob(path.join(scriptdir, '*.hpp')):
    hfile = path.basename(case)
    testname = hfile[:-4]
    cfile = testname + '.cpp'
    temp = tempfile.mkdtemp()
    shutil.copy(path.join(scriptdir, hfile), temp)
    shutil.copy(path.join(scriptdir, cfile), temp)
    shutil.copy(path.join(rtdir, 'ramfuzz-rt.cpp'), temp)
    # Also copy ramfuzz-rt.hpp, but with lower depthlimit so tests don't take
    # forever:
    with open(path.join(rtdir, 'ramfuzz-rt.hpp')) as fsrc:
        newcontent = fsrc.read().replace('depthlimit = 20', 'depthlimit = 4')
        with open(path.join(temp, 'ramfuzz-rt.hpp'), 'w') as fdst:
            fdst.write(newcontent)
    try:
        chdir(temp)
        check_call([path.join(bindir, 'ramfuzz'), hfile, '--', '-std=c++11'])
        build_cmd = [
            path.join(bindir, 'clang++'), '-std=c++11', '-or', '-g', cfile,
            'fuzz.cpp', '-lzmqpp', '-lzmq'
        ]
        check_call(build_cmd)
        endpoint = 'ipc://' + temp + '/socket'
        vgproc = Popen([path.join(bindir, 'valgen'), endpoint])
        check_call([path.join(temp, 'r'), endpoint])
        vgproc.terminate()
        chdir(bindir)  # Just a precaution to guarantee rmtree success.
        shutil.rmtree(path.realpath(temp))
    except CalledProcessError:
        failures += 1
        sys.stderr.write('error in {} ({})\n'.format(testname, temp))
sys.exit(failures)
