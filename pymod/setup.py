#!/usr/bin/env python

# Copyright 2016-2017 The RamFuzz contributors. All rights reserved.
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
"""A distutils setup script for Python's ramfuzz module.  To build and install
the module:

./setup.py build && ./setup.py install

"""

from distutils.core import setup, Extension

module1 = Extension('ramfuzz', sources=['ramfuzzmodule.cpp'])

setup(
    name='ramfuzz',
    version='1.0',
    description='Utilities for processing RamFuzz logs',
    ext_modules=[module1])
