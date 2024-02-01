// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Interfaces/IWindowWidget.h>

namespace mx::gui {

IWindowWidget::IWindowWidget(void) {
  connect(this, &IWindowWidget::RequestAttention, this, &IWindowWidget::show);
}

IWindowWidget::~IWindowWidget(void) {}

void IWindowWidget::hideEvent(QHideEvent *event) {
  QWidget::hideEvent(event);
  emit Hidden();
}

void IWindowWidget::showEvent(QShowEvent *event) {
  QWidget::showEvent(event);
  emit Shown();
}

void IWindowWidget::closeEvent(QCloseEvent *event) {
  QWidget::closeEvent(event);
  emit Closed();
}

void IWindowWidget::EmitRequestAttention(void) {
  emit RequestAttention();
}

}  // namespace mx::gui
