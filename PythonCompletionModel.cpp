// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.
#include <Python.h>

#include "PythonCompletionModel.h"

namespace mx::gui {

struct PythonCompletionModel::PrivateData {
  PyObject *completer_obj;

  PyObject *complete(const char* str, int state) {
    return PyObject_CallMethod(completer_obj, "complete", "si", str, state);
  }

  PrivateData() {
    auto rlcompleter = PyImport_ImportModule("rlcompleter");
    auto completer_class = PyObject_GetAttrString(rlcompleter, "Completer");
    completer_obj = PyObject_CallNoArgs(completer_class);
  }

  ~PrivateData() { Py_DECREF(completer_obj); }
};

PythonCompletionModel::PythonCompletionModel(QObject *parent)
    : QStringListModel(parent), d(std::make_unique<PrivateData>()) {}

PythonCompletionModel::~PythonCompletionModel() = default;

void PythonCompletionModel::setPrefix(const QString& text) {
  auto utf8 = text.toUtf8();
  QStringList list;
  
  int i = 0;
  auto suggestion{d->complete(utf8.data(), i++)};
  while(suggestion != Py_None) {
    auto sugg_str = PyUnicode_AsUTF8(suggestion);
    list.push_back(sugg_str);

    Py_DECREF(suggestion);
    suggestion = d->complete(utf8.data(), i++);
  }
  Py_DECREF(suggestion);
  setStringList(list);
}

} // namespace mx::gui