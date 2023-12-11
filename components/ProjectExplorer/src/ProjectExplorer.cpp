/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "ProjectExplorer.h"

#include <filesystem>
#include <multiplier/ui/Assert.h>

#include <QTreeView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSortFilterProxyModel>
#include <QApplication>
#include <QMenu>
#include <QAction>
#include <QLabel>
#include <QClipboard>

namespace mx::gui {

namespace {

struct ContextMenu final {
  QMenu *menu{nullptr};

  QMenu *copy_menu{nullptr};
  QMenu *sort_menu{nullptr};

  QAction *set_root_action{nullptr};
  QAction *copy_file_name{nullptr};
  QAction *copy_full_path{nullptr};

  QAction *sort_ascending_order{nullptr};
  QAction *sort_descending_order{nullptr};
};

void SaveExpandedNodeListHelper(std::vector<QModelIndex> &expanded_node_list,
                                const QTreeView &tree_view,
                                const QModelIndex &root) {

  const auto &model = *tree_view.model();

  for (int i = 0; i < model.rowCount(root); ++i) {
    auto index = model.index(i, 0, root);

    if (tree_view.isExpanded(index)) {
      expanded_node_list.push_back(index);
    }

    SaveExpandedNodeListHelper(expanded_node_list, tree_view, index);
  }
}

}  // namespace

struct ProjectExplorer::PrivateData final {
  IFileTreeModel *model{nullptr};
  QSortFilterProxyModel *model_proxy{nullptr};
  std::vector<QModelIndex> expanded_node_list;

  QTreeView *tree_view{nullptr};
  ISearchWidget *search_widget{nullptr};
  QWidget *alternative_root_warning{nullptr};

  ContextMenu context_menu;
};

ProjectExplorer::~ProjectExplorer() {}

ProjectExplorer::ProjectExplorer(IFileTreeModel *model, QWidget *parent)
    : IProjectExplorer(parent),
      d(new PrivateData) {

  InitializeWidgets();
  InstallModel(model);
}

void ProjectExplorer::InitializeWidgets() {
  // Setup the tree view
  d->tree_view = new QTreeView();
  d->tree_view->setHeaderHidden(true);
  d->tree_view->setAlternatingRowColors(false);

  d->tree_view->setSelectionBehavior(QAbstractItemView::SelectRows);
  d->tree_view->setAllColumnsShowFocus(true);
  d->tree_view->setTreePosition(0);

  auto indent_width = fontMetrics().horizontalAdvance("_");
  d->tree_view->setIndentation(indent_width);

  d->search_widget = ISearchWidget::Create(ISearchWidget::Mode::Filter, this);
  connect(d->search_widget, &ISearchWidget::SearchParametersChanged, this,
          &ProjectExplorer::OnSearchParametersChange);

  connect(d->search_widget, &ISearchWidget::Activated, this,
          &ProjectExplorer::OnStartSearching);

  connect(d->search_widget, &ISearchWidget::Deactivated, this,
          &ProjectExplorer::OnStopSearching);

  // Create the alternative root item warning
  auto root_warning_label = new QLabel();
  root_warning_label->setTextFormat(Qt::RichText);
  root_warning_label->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
  root_warning_label->setText(tr(
      "A custom root has been set. <a href=\"#set_default_root\">Click here to disable it</a>"));

  auto warning_font = font();
  warning_font.setItalic(true);
  root_warning_label->setFont(warning_font);

  connect(root_warning_label, &QLabel::linkActivated, this,
          &ProjectExplorer::OnDisableCustomRootLinkClicked);

  auto root_warning_layout = new QHBoxLayout();
  root_warning_layout->setContentsMargins(0, 0, 0, 0);
  root_warning_layout->addWidget(root_warning_label);
  root_warning_layout->addStretch();

  d->alternative_root_warning = new QWidget(this);
  d->alternative_root_warning->setLayout(root_warning_layout);

  // Setup the main layout
  setContentsMargins(0, 0, 0, 0);

  auto layout = new QVBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(d->tree_view);
  layout->addWidget(d->search_widget);
  layout->addWidget(d->alternative_root_warning);
  setLayout(layout);

  // Setup che custom context menu
  d->tree_view->setContextMenuPolicy(Qt::CustomContextMenu);

  d->context_menu.menu = new QMenu(tr("Index View menu"));
  d->context_menu.set_root_action = new QAction(tr("Set as root"));
  d->context_menu.menu->addAction(d->context_menu.set_root_action);

  d->context_menu.sort_menu = new QMenu(tr("Sort..."));
  d->context_menu.sort_ascending_order = new QAction(tr("Ascending order"));
  d->context_menu.sort_menu->addAction(d->context_menu.sort_ascending_order);

  d->context_menu.sort_descending_order = new QAction(tr("Descending order"));
  d->context_menu.sort_menu->addAction(d->context_menu.sort_descending_order);

  d->context_menu.menu->addMenu(d->context_menu.sort_menu);

  d->context_menu.copy_menu = new QMenu(tr("Copy..."));
  d->context_menu.copy_file_name = new QAction(tr("File name"));
  d->context_menu.copy_menu->addAction(d->context_menu.copy_file_name);

  d->context_menu.copy_full_path = new QAction(tr("Full path"));
  d->context_menu.copy_menu->addAction(d->context_menu.copy_full_path);

  d->context_menu.menu->addMenu(d->context_menu.copy_menu);

  connect(d->context_menu.menu, &QMenu::triggered, this,
          &ProjectExplorer::OnContextMenuActionTriggered);

  connect(d->tree_view, &QTreeView::customContextMenuRequested, this,
          &ProjectExplorer::OnOpenItemContextMenu);

  connect(&IThemeManager::Get(), &IThemeManager::ThemeChanged, this,
          &ProjectExplorer::OnThemeChange);
}

void ProjectExplorer::InstallModel(IFileTreeModel *model) {
  d->model = model;

  d->model_proxy = new QSortFilterProxyModel(this);
  d->model_proxy->setRecursiveFilteringEnabled(true);
  d->model_proxy->setSourceModel(d->model);
  d->model_proxy->setFilterRole(IFileTreeModel::AbsolutePathRole);
  d->model_proxy->setDynamicSortFilter(true);
  d->model_proxy->sort(0, Qt::AscendingOrder);

  d->tree_view->setModel(d->model_proxy);

  // Note: this needs to happen after the model has been set in the
  // tree view!
  auto tree_selection_model = d->tree_view->selectionModel();
  connect(tree_selection_model, &QItemSelectionModel::currentChanged, this,
          &ProjectExplorer::SelectionChanged);

  connect(d->tree_view, &QTreeView::clicked, this,
          &ProjectExplorer::OnFileTreeItemClicked);

  connect(d->model, &QAbstractItemModel::modelReset, this,
          &ProjectExplorer::OnModelReset);

  OnModelReset();
}

std::vector<QModelIndex> ProjectExplorer::SaveExpandedNodeList() {
  std::vector<QModelIndex> expanded_node_list;
  SaveExpandedNodeListHelper(expanded_node_list, *d->tree_view, QModelIndex());

  for (auto &expanded_node : expanded_node_list) {
    expanded_node = d->model_proxy->mapToSource(expanded_node);
  }

  return expanded_node_list;
}

void ProjectExplorer::ApplyExpandedNodeList(
    const std::vector<QModelIndex> &expanded_node_list) {

  d->tree_view->collapseAll();

  for (auto expanded_node : expanded_node_list) {
    expanded_node = d->model_proxy->mapFromSource(expanded_node);
    d->tree_view->expand(expanded_node);
  }
}

void ProjectExplorer::SelectionChanged(const QModelIndex &index,
                                       const QModelIndex &) {
  OnFileTreeItemClicked(index);
}

void ProjectExplorer::OnFileTreeItemClicked(const QModelIndex &index) {
  auto opt_file_id_var =
      d->model_proxy->data(index, IFileTreeModel::FileIdRole);

  if (!opt_file_id_var.isValid()) {
    return;
  }

  const auto file_id = qvariant_cast<RawEntityId>(opt_file_id_var);
  auto file_name_var = d->model_proxy->data(index);
  auto file_path_var =
      d->model_proxy->data(index, IFileTreeModel::AbsolutePathRole);

  emit FileClicked(file_id, file_name_var.toString(), file_path_var.toString());
}

void ProjectExplorer::OnSearchParametersChange(
    const ISearchWidget::SearchParameters &search_parameters) {

  QRegularExpression::PatternOptions options{
      QRegularExpression::NoPatternOption};

  if (!search_parameters.case_sensitive) {
    options |= QRegularExpression::CaseInsensitiveOption;
  }

  auto pattern = QString::fromStdString(search_parameters.pattern);

  if (search_parameters.type == ISearchWidget::SearchParameters::Type::Text) {
    pattern = QRegularExpression::escape(pattern);
    if (search_parameters.whole_word) {
      pattern = "\\b" + pattern + "\\b";
    }
  }

  QRegularExpression regex(pattern, options);

  // The regex is already validated by the search widget
  Assert(regex.isValid(),
         "Invalid regex found in CodeView::OnSearchParametersChange");

  auto &selection_model = *d->tree_view->selectionModel();
  selection_model.select(QModelIndex(), QItemSelectionModel::Clear);

  d->model_proxy->setFilterRegularExpression(regex);
  d->tree_view->expandRecursively(QModelIndex());
  d->tree_view->resizeColumnToContents(0);
}

void ProjectExplorer::OnOpenItemContextMenu(const QPoint &point) {
  auto index = d->tree_view->indexAt(point);
  if (!index.isValid()) {
    return;
  }

  QVariant action_data;
  action_data.setValue(index);

  auto file_id_role = index.data(IFileTreeModel::FileIdRole);
  auto is_directory = !file_id_role.isValid();
  d->context_menu.set_root_action->setVisible(is_directory);

  for (auto menu : {d->context_menu.menu, d->context_menu.sort_menu,
                    d->context_menu.copy_menu}) {
    for (auto &action : menu->actions()) {
      action->setData(action_data);
    }
  }

  auto menu_position = d->tree_view->viewport()->mapToGlobal(point);
  d->context_menu.menu->exec(menu_position);
}

void ProjectExplorer::OnContextMenuActionTriggered(QAction *action) {
  auto index_var = action->data();
  if (!index_var.isValid()) {
    return;
  }

  const auto &index = qvariant_cast<QModelIndex>(index_var);
  if (!index.isValid()) {
    return;
  }

  if (action == d->context_menu.set_root_action) {
    d->model->SetRoot(index);

  } else if (action == d->context_menu.copy_file_name ||
             action == d->context_menu.copy_full_path) {

    auto file_path_var = index.data(IFileTreeModel::AbsolutePathRole);
    if (file_path_var.isValid()) {
      auto clipboard_value = file_path_var.toString();

      if (action == d->context_menu.copy_file_name) {
        std::filesystem::path path{clipboard_value.toStdString()};
        clipboard_value = QString::fromStdString(path.filename());

        // Use '/' as file name even if the absolute path is the
        // root
        if (clipboard_value.isEmpty()) {
          clipboard_value = file_path_var.toString();
        }
      }

      auto &clipboard = *QGuiApplication::clipboard();
      clipboard.setText(clipboard_value);
    }

  } else if (action == d->context_menu.sort_ascending_order ||
             action == d->context_menu.sort_descending_order) {

    auto sorting_order = (action == d->context_menu.sort_ascending_order)
                             ? Qt::AscendingOrder
                             : Qt::DescendingOrder;

    d->model_proxy->sort(0, sorting_order);
  }
}

void ProjectExplorer::OnModelReset() {
  d->expanded_node_list.clear();

  auto display_root_warning = d->model->HasAlternativeRoot();
  d->alternative_root_warning->setVisible(display_root_warning);

  d->tree_view->expandRecursively(QModelIndex(), 1);
}

void ProjectExplorer::OnDisableCustomRootLinkClicked() {
  d->model->SetDefaultRoot();
}

void ProjectExplorer::OnStartSearching() {
  d->expanded_node_list = SaveExpandedNodeList();
}

void ProjectExplorer::OnStopSearching() {
  ApplyExpandedNodeList(d->expanded_node_list);
  d->expanded_node_list.clear();
}

void ProjectExplorer::OnThemeChange(const QPalette &,
                                    const CodeViewTheme &code_view_theme) {
  QFont font(code_view_theme.font_name);
  setFont(font);
}

}  // namespace mx::gui
