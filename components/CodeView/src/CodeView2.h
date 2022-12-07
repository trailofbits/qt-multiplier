/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/ICodeView2.h>

namespace mx::gui {

class CodeView2 final : public ICodeView2 {
  Q_OBJECT

 public:
  virtual ~CodeView2() override;

 protected:
  CodeView2(ICodeModel *model, QWidget *parent);

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  virtual bool eventFilter(QObject *, QEvent *) override;
  void InstallModel(ICodeModel *model);
  void InitializeWidgets();

 private slots:
  void OnDataChanged();
  void OnCursorPositionChange();
  void OnGutterPaintEvent(QPaintEvent *event);
  void OnTextEditPaintEvent(QPaintEvent *event);
  void OnTextEditUpdateRequest(const QRect &, int);

  friend class ICodeView2;
};

}  // namespace mx::gui
