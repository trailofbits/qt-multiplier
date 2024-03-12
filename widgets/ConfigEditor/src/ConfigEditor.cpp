// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "ConfigEditor.h"
#include "ConfigModel.h"
#include "ConfigEditorDelegate.h"

#include <multiplier/GUI/Widgets/TreeWidget.h>
#include <multiplier/GUI/Widgets/SearchWidget.h>

#include <QSortFilterProxyModel>
#include <QHeaderView>

namespace mx::gui {

QWidget *CreateConfigEditor(const ConfigManager &config_manager,
                            Registry &registry, QWidget *parent) {
  return ConfigEditor::Create(config_manager, registry, parent);
}

struct ConfigEditor::PrivateData final {
  ConfigModel *model{nullptr};
  QSortFilterProxyModel *model_proxy{nullptr};
  TreeWidget *tree_view{nullptr};
  SearchWidget *search_widget{nullptr};
};

ConfigEditor *ConfigEditor::Create(const ConfigManager &config_manager,
                                   Registry &registry, QWidget *parent) {
  return new ConfigEditor(config_manager, registry, parent);
}

ConfigEditor::~ConfigEditor() {}

ConfigEditor::ConfigEditor(const ConfigManager &config_manager,
                           Registry &registry, QWidget *parent)
    : QWidget(parent),
      d(new PrivateData) {

  InitializeWidgets(config_manager, registry);
}

void ConfigEditor::InitializeWidgets(const ConfigManager &config_manager,
                                     Registry &registry) {
  d->model = ConfigModel::Create(registry, this);

  d->model_proxy = new QSortFilterProxyModel(this);
  d->model_proxy->setRecursiveFilteringEnabled(true);
  d->model_proxy->setSourceModel(d->model);
  d->model_proxy->setDynamicSortFilter(true);
  d->model_proxy->sort(0, Qt::AscendingOrder);

  d->tree_view = new TreeWidget(this);
  d->tree_view->setModel(d->model_proxy);
  d->tree_view->header()->hide();
  d->tree_view->setItemDelegateForColumn(
      1, ConfigEditorDelegate::Create(d->tree_view));

  connect(d->tree_view->model(), &QAbstractItemModel::modelReset, this,
          &ConfigEditor::OnModelReset);

  d->search_widget = new SearchWidget(config_manager.MediaManager(),
                                      SearchWidget::Mode::Filter, this);

  connect(d->search_widget, &SearchWidget::SearchParametersChanged, this,
          &ConfigEditor::OnSearchParametersChange);

  setWindowTitle(tr("Configuration"));
  setContentsMargins(0, 0, 0, 0);

  auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(d->tree_view, 1);
  layout->addStretch();
  layout->addWidget(d->search_widget);
  setLayout(layout);

  OnModelReset();
}

void ConfigEditor::OnModelReset() {
  d->tree_view->expandAll();
  d->tree_view->resizeColumnToContents(0);
}

void ConfigEditor::OnSearchParametersChange() {
  auto &search_parameters = d->search_widget->Parameters();
  auto pattern = QString::fromStdString(search_parameters.pattern);
  if (search_parameters.type == SearchWidget::SearchParameters::Type::Text) {
    pattern = QRegularExpression::escape(pattern);
    if (search_parameters.whole_word) {
      pattern = "\\b" + pattern + "\\b";
    }
  }

  auto &selection_model = *d->tree_view->selectionModel();
  selection_model.select(QModelIndex(), QItemSelectionModel::Clear);

  QRegularExpression::PatternOptions options{
      QRegularExpression::NoPatternOption};

  if (!search_parameters.case_sensitive) {
    options |= QRegularExpression::CaseInsensitiveOption;
  }

  QRegularExpression regex(pattern, options);
  d->model_proxy->setFilterRegularExpression(regex);

  d->tree_view->expandRecursively(QModelIndex());
  d->tree_view->resizeColumnToContents(0);
}

}  // namespace mx::gui
