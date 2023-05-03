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
  bool breadcrumbs_enabled{false};
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

  d->code_model = new RefExplorerToCodeViewModelAdapter(model, this);
  d->code_model->SetBreadcrumbsVisibility(d->breadcrumbs_enabled);

  d->code_view = ICodeView::Create(d->code_model, this);

  connect(d->code_view, &ICodeView::TokenTriggered, this,
          &TextBasedReferenceExplorer::OnTokenTriggered);

  auto layout = new QVBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(d->code_view);

  setLayout(layout);
}

void TextBasedReferenceExplorer::OnTokenTriggered(
    const ICodeView::TokenAction &token_action, const QModelIndex &index) {

  auto original_index_var =
      index.data(RefExplorerToCodeViewModelAdapter::OriginalModelIndex);

  if (!original_index_var.isValid()) {
    return;
  }

  auto original_index = qvariant_cast<QModelIndex>(original_index_var);
  if (!original_index.isValid()) {
    return;
  }

  if (token_action.type == ICodeView::TokenAction::Type::Primary) {
    auto is_expand_var =
        index.data(RefExplorerToCodeViewModelAdapter::IsExpandButton);

    if (is_expand_var.isValid() && qvariant_cast<bool>(is_expand_var)) {
      d->model->ExpandEntity(original_index);

    } else {
      emit SelectedItemChanged(original_index);
    }

  } else if (token_action.type == ICodeView::TokenAction::Type::Keyboard) {
    const auto &keyboard_button = token_action.opt_keyboard_button.value();

    if (keyboard_button.key == Qt::Key_Plus) {
      d->model->ExpandEntity(original_index);

    } else if (keyboard_button.key == Qt::Key_Minus) {
      d->model->RemoveEntity(original_index);

    } else if (keyboard_button.key == Qt::Key_Return) {
      emit SelectedItemChanged(original_index);

    } else if (keyboard_button.key == Qt::Key_B) {
      d->breadcrumbs_enabled = !d->breadcrumbs_enabled;
      d->code_model->SetBreadcrumbsVisibility(d->breadcrumbs_enabled);
    }
  }
}

}  // namespace mx::gui
