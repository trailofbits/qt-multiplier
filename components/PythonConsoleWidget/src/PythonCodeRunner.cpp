// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <Python.h>

#include "PythonCodeRunner.h"

namespace mx::gui {

void PythonCodeRunner::run(void) {
  auto ret = PyEval_EvalCode(code, env, env);
  Py_DECREF(code);
  Py_DECREF(env);
  Py_XDECREF(ret);

  if (PyErr_Occurred()) {
    PyErr_Print();
    PyErr_Clear();
  }
  emit EvaluationDone();
}

}  // namespace mx::gui
