// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/Index.h>

#include <QWidget>

#include <memory>

namespace mx {

namespace gui {

class Multiplier;

class PythonPromptView final : public QWidget {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  void InitializeWidgets(void);
 private slots:
  void OnPromptEnter();
  void OnStdOut(const QString& str);
  void OnStdErr(const QString& str);

 public slots:
  void Connected();
  void Disconnected();
  void CurrentFile(mx::RawEntityId id);

 public:
  PythonPromptView(Multiplier& multiplier);
  virtual ~PythonPromptView(void);
};

}  // namespace gui
}  // namespace mx
