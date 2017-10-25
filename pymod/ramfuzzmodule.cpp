// Copyright 2016-2017 The RamFuzz contributors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <Python.h>

#include <iostream>
#include <unistd.h>

using namespace std;

/// Reads a T value from RamFuzz log opened under the file descriptor fd.  After
/// reading the value, reads its id and returns a Python tuple (value, id).
template <typename T> PyObject *logread(int fd) {
  T val;
  if (read(fd, &val, sizeof(val)) < sizeof(val))
    return Py_BuildValue("");
  size_t id;
  if (read(fd, &id, sizeof(id)) < sizeof(id))
    return Py_BuildValue("");
  unsigned long long lid(id);
  return Py_BuildValue("d K", double(val), lid);
}

/// Implements Python's ramfuzz.load(), which is documented below in \c methods.
static PyObject *ramfuzz_load(PyObject *self, PyObject *args) {
  int fd;
  if (!PyArg_ParseTuple(args, "i", &fd) || fd < 0)
    return NULL;
  char tag;
  read(fd, &tag, 1);
  switch (tag) {
  // The following must match the specializations of
  // ramfuzz::runtime::typetag.
  case 0:
    return logread<bool>(fd);
  case 1:
    return logread<char>(fd);
  case 2:
    return logread<unsigned char>(fd);
  case 3:
    return logread<short>(fd);
  case 4:
    return logread<unsigned short>(fd);
  case 5:
    return logread<int>(fd);
  case 6:
    return logread<unsigned int>(fd);
  case 7:
    return logread<long>(fd);
  case 8:
    return logread<unsigned long>(fd);
  case 9:
    return logread<long long>(fd);
  case 10:
    return logread<unsigned long long>(fd);
  case 11:
    return logread<float>(fd);
  case 12:
    return logread<double>(fd);
  default:
    return NULL;
  }
}

/// A list of all methods in this module.
static PyMethodDef methods[] = {
    {"load", ramfuzz_load, METH_VARARGS,
     "Return the next value from the RamFuzz log whose file descriptor is "
     "passed as the sole (int) argument."},
    {NULL, NULL, 0, NULL} /* Sentinel */
};

/// Module initialization.
PyMODINIT_FUNC initramfuzz(void) { (void)Py_InitModule("ramfuzz", methods); }
