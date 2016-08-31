#!/usr/bin/python
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
from subprocess import CalledProcessError, check_call
import shutil
import sys
import tempfile

if len(sys.argv) != 2:
    sys.exit('usage: %s <llvm-build-dir>' % sys.argv[0])
bindir = path.join(sys.argv[1], 'bin')
scriptdir = path.dirname(path.realpath(__file__))
rtdir = path.join(scriptdir, '..', 'runtime')
for case in glob(path.join(scriptdir, '*.hpp')):
    hfile = path.basename(case)
    testname = hfile[:-4]
    cfile = testname + '.cpp'
    temp = tempfile.mkdtemp()
    shutil.copy(path.join(rtdir, 'ramfuzz-rt.hpp'), temp)
    shutil.copy(path.join(rtdir, 'ramfuzz-rt.cpp'), temp)
    shutil.copy(path.join(scriptdir, hfile), temp)
    shutil.copy(path.join(scriptdir, cfile), temp)
    try:
        chdir(temp)
        check_call([path.join(bindir, 'ramfuzz'), hfile, '--', '-std=c++11'])
        check_call([path.join(bindir, 'clang++'), '-std=c++11', '-or', '-g',
                     cfile, 'fuzz.cpp', 'ramfuzz-rt.cpp'])
        check_call(path.join(temp, 'r'))
        chdir(bindir) # Just a precaution to guarantee rmtree success.
        shutil.rmtree(path.realpath(temp))
    except CalledProcessError:
        sys.exit('error in {} ({})'.format(testname,temp))
