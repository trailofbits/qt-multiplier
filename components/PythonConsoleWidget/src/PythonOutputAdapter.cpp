// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <Python.h>

#include "PythonOutputAdapter.h"

#include <multiplier/Bindings/Python.h>
#include <multiplier/Frontend/Token.h>

namespace mx::gui {

struct PythonOutputAdapterWrapper final : public ::PyObject {
  PythonOutputAdapter *adapter;

  static PyObject *DoWrite(BorrowedPyObject *self,
                           BorrowedPyObject * const *args,
                           int num_args);
};

namespace {

static bool gTypeInitialized = false;

static PyMethodDef gMethods[] = {
  {"write",
   reinterpret_cast<PyCFunction>(PythonOutputAdapterWrapper::DoWrite),
   METH_FASTCALL,
   PyDoc_STR("Write the data to the stream")},
  {}
};

static PyTypeObject gType = {
  PyVarObject_HEAD_INIT(NULL, 0)
  .tp_name = "MultiplierPythonOutputAdapter",
  .tp_doc = PyDoc_STR("Output wrapper for the Multiplier GUI's Python console."),
  .tp_basicsize = sizeof(PythonOutputAdapterWrapper),
  .tp_itemsize = 0,
  .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DISALLOW_INSTANTIATION,
  .tp_members = nullptr,
  .tp_methods = gMethods,
};

}  // namespace

PyObject *PythonOutputAdapterWrapper::DoWrite(
    BorrowedPyObject *self, BorrowedPyObject * const *args, int num_args) {
  
  if (!num_args || num_args > 1) {
    PyErr_Format(PyExc_TypeError, "Invalid number of arguments to 'write'");
    return nullptr;
  }

  std::string data;

  if (auto as_token_range = ::mx::from_python<::mx::TokenRange>(args[0])) {
    auto range_data = as_token_range->data();
    data.insert(data.end(), range_data.begin(), range_data.end());

  } else if (auto as_token = ::mx::from_python<::mx::Token>(args[0])) {
    auto tok_data = as_token->data();
    data.insert(data.end(), tok_data.begin(), tok_data.end());

  } else if (auto as_string = ::mx::from_python<std::string>(args[0])) {
    data = std::move(as_string.value());

  } else {
    PyErr_Format(PyExc_TypeError, "Invalid argument type to 'write'");
    return nullptr;
  }
  
  auto wrapper = reinterpret_cast<PythonOutputAdapterWrapper *>(self);
  wrapper->adapter->Write(std::move(data));

  Py_RETURN_NONE;
}

struct PythonOutputAdapter::PrivateData {
  
  PythonOutputAdapterWrapper *obj{nullptr};

  PrivateData(PythonOutputAdapter *adapter);

  ~PrivateData(void) {
    Py_DECREF(obj);
  }
};

PythonOutputAdapter::PrivateData::PrivateData(PythonOutputAdapter *adapter) {
  if (!gTypeInitialized) {
    (void) PyType_Ready(&gType);
    
    obj = reinterpret_cast<PythonOutputAdapterWrapper *>(
        gType.tp_alloc(&gType, 0));
    obj->adapter = adapter;
  }
}

PythonOutputAdapter::PythonOutputAdapter(void)
    : d(std::make_unique<PrivateData>(this)) {}

PythonOutputAdapter::~PythonOutputAdapter(void) {}

void PythonOutputAdapter::Write(std::string data) {
  emit OnWrite(QString::fromStdString(data));
}

// Return a new reference to the I/O object.
PyObject *PythonOutputAdapter::GetInstance(void) const {
  Py_INCREF(d->obj);
  return d->obj;
}

}  // namespace mx::gui
