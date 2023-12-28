// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QObject>

#include <memory>

typedef struct _object PyObject;

namespace mx::gui {

struct PythonOutputAdapterWrapper;

class PythonOutputAdapter final : public QObject {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  void Write(std::string data);

  friend struct PythonOutputAdapterWrapper;

 public:
  PythonOutputAdapter(void);
  virtual ~PythonOutputAdapter(void);

  // Return a new reference to the I/O object.
  PyObject *GetInstance(void) const;

 signals:
  void OnWrite(const QString &str);
};

}  // namespace mx::gui
