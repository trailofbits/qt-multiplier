/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "FileTreeView.h"

#include <filesystem>

#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/ThemeManager.h>
#include <multiplier/GUI/Widgets/SearchWidget.h>

#include <QTreeView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSortFilterProxyModel>
#include <QLabel>
#include <QMouseEvent>

#include "FileTreeModel.h"

namespace mx::gui {
namespace {

// Activate the selected index when pressing this key
static constexpr auto kActivateSelectedItem{Qt::Key_Return};

// Allow users to avoid activating an item with a click by holding this key down
static constexpr auto kDisableClickActivationModifier{Qt::ControlModifier};

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

struct FileTreeView::PrivateData final {
  FileTreeModel *model{nullptr};
  QSortFilterProxyModel *model_proxy{nullptr};
  std::vector<QModelIndex> expanded_node_list;

  QTreeView *tree_view{nullptr};
  SearchWidget *search_widget{nullptr};
  QWidget *alternative_root_warning{nullptr};

  QModelIndex requested_index;
};

FileTreeView::~FileTreeView(void) {}

FileTreeView::FileTreeView(const ConfigManager &config_manager,
                           FileTreeModel *model,
                           QWidget *parent)
    : IWindowWidget(parent),
      d(new PrivateData) {

  InitializeWidgets(config_manager);
  InstallModel(model);
}

bool FileTreeView::eventFilter(QObject *object, QEvent *event) {
  if (object == d->tree_view->viewport()) {
    auto me = dynamic_cast<QMouseEvent *>(event);
    if (!me) {
      return false;
    }

    auto local_mouse_pos = me->position().toPoint();

    auto index = d->tree_view->indexAt(local_mouse_pos);
    if (!index.isValid()) {
      return false;
    }

    // Detect if we're in the item, or in the whitespace/decoration before
    // the item.
    auto rect = d->tree_view->visualRect(index);
    if (!rect.contains(local_mouse_pos)) {
      return false;
    }

    if (event->type() == QEvent::MouseButtonPress) {
      return true;
    }

    if (event->type() != QEvent::MouseButtonRelease) {
      return false;
    }

    auto &selection_model = *d->tree_view->selectionModel();
    selection_model.setCurrentIndex(index,
        QItemSelectionModel::Clear | QItemSelectionModel::SelectCurrent);

    switch (me->button()) {
      case Qt::LeftButton: {
        if ((me->modifiers() & kDisableClickActivationModifier) == 0) {
          OnFileTreeItemActivated(index);
        }

        break;
      }

      case Qt::RightButton: {
        OnOpenItemContextMenu(local_mouse_pos);
        break;
      }

      default:
        break;
    }

    return true;

  } else if (object == d->tree_view) {
    if (event->type() == QEvent::KeyRelease) {
      const auto &selection_model = *d->tree_view->selectionModel();
      auto index = selection_model.currentIndex();
      if (!index.isValid()) {
        return false;
      }

      const auto &key_event = *static_cast<QKeyEvent *>(event);
      switch (key_event.keyCombination().key()) {
      case kActivateSelectedItem:
        OnFileTreeItemActivated(index);
        return true;

      default:
        return false;
      }

    } else {
      return false;
    }

  } else {
    return false;
  }
}

void FileTreeView::InitializeWidgets(const ConfigManager &config_manager) {
  auto &theme_manager = config_manager.ThemeManager();
  auto &media_manager = config_manager.MediaManager();

  // Setup the tree view
  d->tree_view = new QTreeView;
  d->tree_view->setHeaderHidden(true);
  d->tree_view->setAlternatingRowColors(false);

  d->tree_view->setSelectionBehavior(QAbstractItemView::SelectRows);
  d->tree_view->setTextElideMode(Qt::ElideMiddle);
  d->tree_view->setAllColumnsShowFocus(true);
  d->tree_view->setTreePosition(0);

  d->search_widget = new SearchWidget(media_manager, SearchWidget::Mode::Filter,
                                      this);
  connect(d->search_widget, &SearchWidget::SearchParametersChanged,
          this, &FileTreeView::OnSearchParametersChange);

  connect(d->search_widget, &SearchWidget::Activated,
          this, &FileTreeView::OnStartSearching);

  connect(d->search_widget, &SearchWidget::Deactivated,
          this, &FileTreeView::OnStopSearching);

  // Create the alternative root item warning
  auto root_warning_label = new QLabel;
  root_warning_label->setTextFormat(Qt::RichText);
  root_warning_label->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
  root_warning_label->setText(tr(
      "A custom root has been set. <a href=\"#set_default_root\">Click here to disable it</a>"));

  auto warning_font = font();
  warning_font.setItalic(true);
  root_warning_label->setFont(warning_font);

  connect(root_warning_label, &QLabel::linkActivated,
          this, &FileTreeView::OnDisableCustomRootLinkClicked);

  auto root_warning_layout = new QHBoxLayout(this);
  root_warning_layout->setContentsMargins(0, 0, 0, 0);
  root_warning_layout->addWidget(root_warning_label);
  root_warning_layout->addStretch();

  d->alternative_root_warning = new QWidget(this);
  d->alternative_root_warning->setLayout(root_warning_layout);

  // Setup the main layout
  setContentsMargins(0, 0, 0, 0);

  auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(d->tree_view, 1);
  layout->addStretch();
  layout->addWidget(d->search_widget);
  layout->addWidget(d->alternative_root_warning);
  setLayout(layout);

  d->tree_view->installEventFilter(this);
  d->tree_view->viewport()->installEventFilter(this);

  connect(&theme_manager, &ThemeManager::ThemeChanged,
          this, &FileTreeView::OnThemeChanged);

  OnThemeChanged(theme_manager);

  config_manager.InstallItemDelegate(d->tree_view);
}

void FileTreeView::InstallModel(FileTreeModel *model) {
  d->model = model;

  d->model_proxy = new QSortFilterProxyModel(this);
  d->model_proxy->setRecursiveFilteringEnabled(true);
  d->model_proxy->setSourceModel(d->model);
  d->model_proxy->setFilterRole(FileTreeModel::AbsolutePathRole);
  d->model_proxy->setDynamicSortFilter(true);
  d->model_proxy->sort(0, Qt::AscendingOrder);

  d->tree_view->setModel(d->model_proxy);

  connect(d->model, &QAbstractItemModel::modelReset,
          this, &FileTreeView::OnModelReset);

  OnModelReset();
}

std::vector<QModelIndex> FileTreeView::SaveExpandedNodeList(void) {
  std::vector<QModelIndex> expanded_node_list;
  SaveExpandedNodeListHelper(expanded_node_list, *d->tree_view, QModelIndex());

  for (auto &expanded_node : expanded_node_list) {
    expanded_node = d->model_proxy->mapToSource(expanded_node);
  }

  return expanded_node_list;
}

void FileTreeView::ApplyExpandedNodeList(
    std::vector<QModelIndex> expanded_node_list) {

  d->tree_view->collapseAll();

  for (auto expanded_node : expanded_node_list) {
    expanded_node = d->model_proxy->mapFromSource(expanded_node);
    d->tree_view->expand(expanded_node);
  }
}

void FileTreeView::OnFileTreeItemActivated(const QModelIndex &index) {
  d->requested_index = {};

  auto orig_index = d->model_proxy->mapToSource(index);
  if (!orig_index.isValid()) {
    return;
  }

  auto opt_file_id_var = orig_index.data(FileTreeModel::FileIdRole);
  if (!opt_file_id_var.isValid()) {
    return;
  }

  d->requested_index = orig_index;
  emit RequestPrimaryClick(d->requested_index);
}

void FileTreeView::OnSearchParametersChange(void) {

  auto &search_parameters = d->search_widget->Parameters();

  QRegularExpression::PatternOptions options{
      QRegularExpression::NoPatternOption};

  if (!search_parameters.case_sensitive) {
    options |= QRegularExpression::CaseInsensitiveOption;
  }

  auto pattern = QString::fromStdString(search_parameters.pattern);

  if (search_parameters.type == SearchWidget::SearchParameters::Type::Text) {
    pattern = QRegularExpression::escape(pattern);
    if (search_parameters.whole_word) {
      pattern = "\\b" + pattern + "\\b";
    }
  }

  QRegularExpression regex(pattern, options);

  // The regex is already validated by the search widget
  Q_ASSERT(regex.isValid());

  auto &selection_model = *d->tree_view->selectionModel();
  selection_model.select(QModelIndex(), QItemSelectionModel::Clear);

  d->model_proxy->setFilterRegularExpression(regex);
  d->tree_view->expandRecursively(QModelIndex());
  d->tree_view->resizeColumnToContents(0);
}

//! Sets the root index.
void FileTreeView::SetRoot(const QModelIndex &index) {
  d->model->SetRoot(index);
}

void FileTreeView::SortAscending(void) {
  d->model_proxy->sort(0, Qt::AscendingOrder);
}

void FileTreeView::SortDescending(void) {
  d->model_proxy->sort(0, Qt::DescendingOrder);
}

void FileTreeView::OnOpenItemContextMenu(const QPoint &tree_local_mouse_pos) {
  auto index = d->tree_view->indexAt(tree_local_mouse_pos);
  auto orig_index = d->model_proxy->mapToSource(index);

  d->requested_index = orig_index;
  if (!d->requested_index.isValid()) {
    return;
  }

  emit RequestSecondaryClick(d->requested_index);
}

void FileTreeView::OnModelReset(void) {
  d->expanded_node_list.clear();

  auto display_root_warning = d->model->HasAlternativeRoot();
  d->alternative_root_warning->setVisible(display_root_warning);

  d->tree_view->expandRecursively(QModelIndex(), 1);
}

void FileTreeView::OnDisableCustomRootLinkClicked(void) {
  d->model->SetDefaultRoot();
}

void FileTreeView::OnStartSearching(void) {
  d->expanded_node_list = SaveExpandedNodeList();
}

void FileTreeView::OnStopSearching(void) {
  ApplyExpandedNodeList(std::move(d->expanded_node_list));
}

//! Called by the theme manager
void FileTreeView::OnThemeChanged(const ThemeManager &theme_manager) {
  setFont(theme_manager.Theme()->Font());
}

}  // namespace mx::gui
