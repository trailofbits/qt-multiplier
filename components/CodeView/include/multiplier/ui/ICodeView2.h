/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/CodeViewTheme.h>
#include <multiplier/ui/ICodeModel.h>

#include <QWidget>

namespace mx::gui {

class ICodeView2 : public QWidget {
  Q_OBJECT

 public:
  static ICodeView2 *Create(ICodeModel *model, QWidget *parent = nullptr);
  virtual void setTheme(const CodeViewTheme &theme) = 0;

  ICodeView2(QWidget *parent) : QWidget(parent) {}
  virtual ~ICodeView2() override = default;

  ICodeView2(const ICodeView2 &) = delete;
  ICodeView2 &operator=(const ICodeView2 &) = delete;

 signals:
  void TokenClicked(const CodeModelIndex &index, Qt::MouseButtons mouse_buttons,
                    bool double_click);

  void TokenHovered(const CodeModelIndex &index);
};

}  // namespace mx::gui
