// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once
#include <QObject>

#include <memory>

// Not elegant, but including the public Python header causes issues
typedef struct _object PyObject;
typedef struct PyMethodDef PyMethodDef;
typedef struct _typeobject PyTypeObject;

namespace mx {

namespace gui {

class PythonOutputAdapter final : public QObject {
  Q_OBJECT

  static PyObject* py_write(PyObject* self, PyObject* args);
  static PyMethodDef methods[];
  static PyTypeObject type;

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  void write(const QString& str);

 public:
  PythonOutputAdapter();
  virtual ~PythonOutputAdapter(void);

  PyObject *GetInstance();

  static bool InitPythonType();

  static PythonOutputAdapter *StdOut;
  static PythonOutputAdapter *StdErr;

 signals:
  void OnWrite(const QString& str);
};

}  // namespace gui
}  // namespace mx
