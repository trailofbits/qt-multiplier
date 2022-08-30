// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <Python.h>
#include <structmember.h>

#include "PythonOutputAdapter.h"

namespace mx::gui {

struct Wrapper {
  PyObject_HEAD
  PythonOutputAdapter* adapter;
};

PyObject* PythonOutputAdapter::py_write(PyObject *self, PyObject *args)
{
  const char *what;
  if (!PyArg_ParseTuple(args, "s", &what)) {
    return nullptr;
  }
  auto wrapper = reinterpret_cast<Wrapper*>(self);
  wrapper->adapter->write(what);
  Py_RETURN_NONE;
}

static PyMemberDef members[] = {{}};

PyMethodDef PythonOutputAdapter::methods[] = {
  {"write", PythonOutputAdapter::py_write, METH_VARARGS, ""},
  {}
};

PyTypeObject PythonOutputAdapter::type = {
  PyVarObject_HEAD_INIT(NULL, 0)
  .tp_name = "_multiplier.PythonOutputAdapter",
  .tp_doc = PyDoc_STR(""),
  .tp_basicsize = sizeof(Wrapper),
  .tp_itemsize = 0,
  .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
  .tp_members = members,
  .tp_methods = methods,
};

struct PythonOutputAdapter::PrivateData {
  PyObject* obj;
};

PythonOutputAdapter::PythonOutputAdapter()
    : d{std::make_unique<PrivateData>()} {
  auto wrapper = reinterpret_cast<Wrapper*>(type.tp_alloc(&type, 0));
  wrapper->adapter = this;
  d->obj = reinterpret_cast<PyObject*>(wrapper);
}

PythonOutputAdapter::~PythonOutputAdapter(void) {}

void PythonOutputAdapter::write(const QString& str) {
  emit OnWrite(str);
}

bool PythonOutputAdapter::InitPythonType() {
  if(PyType_Ready(&type)) {
    return false;
  }
  Py_INCREF(&type);
  return true;
}

PyObject* PythonOutputAdapter::GetInstance() {
  return d->obj;
}

PythonOutputAdapter* PythonOutputAdapter::StdOut;
PythonOutputAdapter* PythonOutputAdapter::StdErr;

}  // namespace mx::gui
