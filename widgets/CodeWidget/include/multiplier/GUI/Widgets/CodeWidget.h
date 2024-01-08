/*
  Copyright (c) 2024-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QWidget>

#include <memory>
#include <multiplier/Frontend/TokenTree.h>

namespace mx::gui {

class CodeWidget Q_DECL_FINAL : public QWidget {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  virtual ~CodeWidget(void);

  CodeWidget(QWidget *parent = nullptr);

  void SetTokenTree(TokenTree range);
};

}  // namespace mx::gui
