// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "ConfigEditor.h"
#include "ConfigModel.h"
#include "ConfigEditorDelegate.h"

#include <multiplier/GUI/Widgets/TreeWidget.h>

#include <QHeaderView>

namespace mx::gui {

QWidget *CreateConfigEditor(Registry &registry, QWidget *parent) {
  return ConfigEditor::Create(registry, parent);
}

struct ConfigEditor::PrivateData final {
  TreeWidget *tree_view{nullptr};
};

ConfigEditor *ConfigEditor::Create(Registry &registry, QWidget *parent) {
  return new ConfigEditor(registry, parent);
}

ConfigEditor::~ConfigEditor() {}

ConfigEditor::ConfigEditor(Registry &registry, QWidget *parent)
    : QWidget(parent),
      d(new PrivateData) {

  InitializeWidgets(registry);
}

void ConfigEditor::InitializeWidgets(Registry &registry) {
  d->tree_view = new TreeWidget(this);
  d->tree_view->header()->hide();
  d->tree_view->setModel(ConfigModel::Create(registry, d->tree_view));
  d->tree_view->setEditTriggers(QAbstractItemView::AllEditTriggers);
  d->tree_view->setItemDelegateForColumn(
      1, ConfigEditorDelegate::Create(d->tree_view));

  connect(d->tree_view->model(), &QAbstractItemModel::modelReset, this,
          &ConfigEditor::OnModelReset);

  OnModelReset();

  auto layout = new QVBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(d->tree_view);

  setWindowTitle(tr("Configuration"));
  setContentsMargins(0, 0, 0, 0);
  setLayout(layout);
}

void ConfigEditor::OnModelReset() {
  d->tree_view->expandAll();
  d->tree_view->resizeColumnToContents(0);
}

}  // namespace mx::gui
