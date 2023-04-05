/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "TextBasedReferenceExplorer.h"
#include "RefExplorerToCodeViewModelAdapter.h"

#include <QVBoxLayout>

namespace mx::gui {

struct TextBasedReferenceExplorer::PrivateData final {
  IReferenceExplorerModel *model{nullptr};

  RefExplorerToCodeViewModelAdapter *code_model{nullptr};
  ICodeView *code_view{nullptr};
};

TextBasedReferenceExplorer::~TextBasedReferenceExplorer() {}

IReferenceExplorerModel *TextBasedReferenceExplorer::Model() {
  return d->model;
}

TextBasedReferenceExplorer::TextBasedReferenceExplorer(
    IReferenceExplorerModel *model, QWidget *parent)
    : IReferenceExplorer(parent),
      d(new PrivateData) {

  InitializeWidgets(model);
}

void TextBasedReferenceExplorer::InitializeWidgets(
    IReferenceExplorerModel *model) {
  setContentsMargins(0, 0, 0, 0);

  d->model = model;
  d->model->setParent(this);

  d->code_model = new RefExplorerToCodeViewModelAdapter(model, this);
  d->code_view = ICodeView::Create(d->code_model, this);

  connect(d->code_view, &ICodeView::TokenTriggered,
          this, &TextBasedReferenceExplorer::OnTokenTriggered);

  connect(d->code_view, &ICodeView::CursorMoved,
          this, &TextBasedReferenceExplorer::OnCursorMoved);

  auto layout = new QVBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(d->code_view);

  setLayout(layout);
}

void TextBasedReferenceExplorer::OnCursorMoved(const CodeModelIndex &index) {
  auto original_index_var = d->code_model->Data(
      index, RefExplorerToCodeViewModelAdapter::OriginalModelIndex);

  if (!original_index_var.isValid()) {
    return;
  }

  auto original_index = qvariant_cast<QModelIndex>(original_index_var);
  if (!original_index.isValid()) {
    return;
  }

  emit SelectedItemChanged(original_index);
}

void TextBasedReferenceExplorer::OnTokenTriggered(
    const ICodeView::TokenAction &token_action, const CodeModelIndex &index) {

  auto original_index_var = d->code_model->Data(
      index, RefExplorerToCodeViewModelAdapter::OriginalModelIndex);

  if (!original_index_var.isValid()) {
    return;
  }

  auto original_index = qvariant_cast<QModelIndex>(original_index_var);
  if (!original_index.isValid()) {
    return;
  }

  if (token_action.type == ICodeView::TokenAction::Type::Primary) {
    emit ItemActivated(original_index);

  } else if (token_action.type == ICodeView::TokenAction::Type::Keyboard) {

    const auto &keyboard_button = token_action.opt_keyboard_button.value();

    if (keyboard_button.key == Qt::Key_Plus) {
      d->model->ExpandEntity(original_index);

    } else if (keyboard_button.key == Qt::Key_Minus) {
      d->model->RemoveEntity(original_index);

    } else if (keyboard_button.key == Qt::Key_Return) {
      emit SelectedItemChanged(original_index);
    }
  }
}

}  // namespace mx::gui
