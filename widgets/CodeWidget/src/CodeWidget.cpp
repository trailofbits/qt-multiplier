/*
  Copyright (c) 2024-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/GUI/Widgets/CodeWidget.h>

#include <QTextEdit>
#include <QVBoxLayout>

#include "CodeModel.h"

namespace mx::gui {

struct CodeWidget::PrivateData {
  CodeModel *model{nullptr};
  QTextEdit *view{nullptr};
};

CodeWidget::~CodeWidget(void) {}

CodeWidget::CodeWidget(QWidget *parent)
    : QWidget(parent),
      d(new PrivateData) {
  d->model = new CodeModel(this);
  d->view = new QTextEdit(this);

  auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(d->view, 1);
  layout->addStretch();

  setContentsMargins(0, 0, 0, 0);
  setLayout(layout);
}

void CodeWidget::SetTokenTree(TokenTree tree) {
  d->view->setDocument(d->model->Import(std::move(tree)));
}

}  // namespace mx::gui
