// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QStringListModel>

#include <memory>

namespace mx::gui {

class PythonCompletionModel final : public QStringListModel {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  virtual ~PythonCompletionModel(void);

  PythonCompletionModel(QObject *parent=nullptr);

  void enableSuggestions(void);

 public slots:
  void setPrefix(const QString &text);
};

}  // namespace mx::gui
