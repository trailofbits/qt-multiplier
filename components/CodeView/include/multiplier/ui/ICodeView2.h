/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/ICodeModel.h>

#include <QWidget>

namespace mx {

class ICodeView2 : public QWidget {
  Q_OBJECT

 public:
  // Other compatible roles:
  // Qt::ToolTipRole
  // Qt::DisplayRole
  // Qt::ForegroundRole
  // Qt::BackgroundRole
  // Qt::FontRole
  enum {
    ItalicStyleRole = Qt::UserRole,
    BoldStyleRole,
    UnderlineStyleRole,
    StrikeOutStyleRole,
    TokenIdRole,
    RowNumberRole,
    FileNameRole,
    CollapsibleRole,
    TokenClassRole,
  };

  static ICodeView2 *Create(ICodeModel *model, QWidget *parent = nullptr);

  ICodeView2(QWidget *parent) : QWidget(parent) {}
  virtual ~ICodeView2() override = default;

  ICodeView2(const ICodeView2 &) = delete;
  ICodeView2 &operator=(const ICodeView2 &) = delete;
};

}  // namespace mx
