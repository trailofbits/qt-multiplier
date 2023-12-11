// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <Python.h>

#include "PythonCompletionModel.h"

namespace mx::gui {

struct PythonCompletionModel::PrivateData {

  PyObject *completer_obj{nullptr};
  QStringList suggestions;

  PrivateData(void);
  ~PrivateData(void);

  void FillSuggestions(const char *str);
};

PythonCompletionModel::PrivateData::PrivateData(void) {
  auto rlcompleter = PyImport_ImportModule("rlcompleter");
  auto completer_class = PyObject_GetAttrString(rlcompleter, "Completer");
  Py_DECREF(rlcompleter);

  completer_obj = PyObject_CallNoArgs(completer_class);
  assert(PyObject_IsInstance(completer_obj, completer_class));
  Py_DECREF(completer_class);
}

PythonCompletionModel::PrivateData::~PrivateData(void) {
  Py_DECREF(completer_obj);
}

void PythonCompletionModel::PrivateData::FillSuggestions(const char *str) {
  suggestions.clear();
  for (int i = 0; ; ++i) {
    auto suggestion = PyObject_CallMethod(
        completer_obj, "complete", "si", str, i);
    if (!suggestion) {
      break;
    }

    if (suggestion == Py_None) {
      Py_DECREF(suggestion);
      break;
    }

    suggestions.push_back(PyUnicode_AsUTF8(suggestion));
    Py_DECREF(suggestion);
  }
}

PythonCompletionModel::PythonCompletionModel(QObject *parent)
    : QStringListModel(parent),
      d(std::make_unique<PrivateData>()) {}

PythonCompletionModel::~PythonCompletionModel() = default;

void PythonCompletionModel::setPrefix(const QString &text) {
  auto utf8 = text.toUtf8();
  d->FillSuggestions(utf8.data());
  setStringList(d->suggestions);
}

void PythonCompletionModel::enableSuggestions(void) {
  setStringList(d->suggestions);
}

} // namespace mx::gui
