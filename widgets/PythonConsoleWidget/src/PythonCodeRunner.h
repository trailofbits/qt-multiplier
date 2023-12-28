// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QObject>
#include <QRunnable>

typedef struct _object PyObject;

namespace mx::gui {

class PythonCodeRunner final : public QObject, public QRunnable {
  Q_OBJECT

  PyObject * const code;
  PyObject * const env;

 public:
  inline PythonCodeRunner(PyObject *code_, PyObject *env_)
      : code(code_),
        env(env_) {}
  
  void run(void) final;

 signals:
  void EvaluationDone(void);
};

}  // namespace mx::gui