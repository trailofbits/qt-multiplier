/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "TreeExplorer.h"
#include "FilterSettingsWidget.h"
#include "TreeExplorerItemDelegate.h"
#include "TreeExplorerModel.h"
#include "TreeExplorerTreeView.h"
#include "SearchFilterModelProxy.h"

#include <multiplier/ui/Assert.h>
#include <multiplier/ui/Icons.h>
#include <multiplier/ui/IGlobalHighlighter.h>

#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QDropEvent>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QVBoxLayout>
#include <QScrollBar>
#include <QAbstractItemModelTester>
#include <QLoggingCategory>

#include <optional>

namespace mx::gui {

namespace {

struct ContextMenu final {
  QMenu *menu{nullptr};
  QAction *copy_details_action{nullptr};
};

struct TreeviewItemButtons final {
  std::optional<QModelIndex> opt_hovered_index;

  bool updating_buttons{false};

  // Keep up to date with UpdateTreeViewItemButtons
  QPushButton *open{nullptr};
  QPushButton *expand{nullptr};
  QPushButton *goto_{nullptr};
};

}  // namespace

struct TreeExplorer::PrivateData final {
  ITreeExplorerModel *model{nullptr};
  QAbstractItemModelTester *model_tester{nullptr};
  SearchFilterModelProxy *model_proxy{nullptr};
  QTreeView *tree_view{nullptr};
  ISearchWidget *search_widget{nullptr};
  FilterSettingsWidget *filter_settings_widget{nullptr};
  ContextMenu context_menu;
  QWidget *status_widget{nullptr};
  TreeviewItemButtons treeview_item_buttons;
};

TreeExplorer::~TreeExplorer() {}

TreeExplorer::TreeExplorer(ITreeExplorerModel *model,
                           QWidget *parent,
                           IGlobalHighlighter *global_highlighter)
    : ITreeExplorer(parent),
      d(new PrivateData) {

  QLoggingCategory::setFilterRules("qt.modeltest=true");
  d->model_tester = new QAbstractItemModelTester(
      model, QAbstractItemModelTester::FailureReportingMode::Fatal, this);

  InitializeWidgets(model);
  InstallModel(model, global_highlighter);

  // Synchronize the search widget and its addon
  d->search_widget->Deactivate();
}

void TreeExplorer::InitializeWidgets(ITreeExplorerModel *model) {

  // Initialize the tree view
  d->tree_view = new QTreeView(this);

  d->tree_view->setSortingEnabled(true);
  d->tree_view->sortByColumn(0, Qt::AscendingOrder);

  // The auto scroll takes care of keeping the active item within the
  // visible viewport region. This is true for mouse clicks but also
  // keyboard navigation (i.e. arrow keys, page up/down, etc).
  d->tree_view->setAutoScroll(false);

  // Smooth scrolling.
  d->tree_view->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  d->tree_view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

  // We'll potentially have a bunch of columns depending on the configuration,
  // so make sure they span to use all available space.
  QHeaderView *header = d->tree_view->header();
  header->setStretchLastSection(true);

  // Don't let double click expand things in three; we capture double click so
  // that we can make it open up the use in the code.
  d->tree_view->setExpandsOnDoubleClick(false);

  // Disallow multiple selection. If we have grouping by file enabled, then when
  // a user clicks on a file name, we instead jump down to the first entry
  // grouped under that file. This is to make using the up/down arrows easier.
  d->tree_view->setSelectionBehavior(QAbstractItemView::SelectRows);
  d->tree_view->setSelectionMode(QAbstractItemView::SingleSelection);
  d->tree_view->setAllColumnsShowFocus(true);
  d->tree_view->setTreePosition(0);
  d->tree_view->setTextElideMode(Qt::TextElideMode::ElideRight);

  d->tree_view->setAlternatingRowColors(false);
  d->tree_view->setContextMenuPolicy(Qt::CustomContextMenu);
  d->tree_view->installEventFilter(this);
  d->tree_view->viewport()->installEventFilter(this);
  d->tree_view->viewport()->setMouseTracking(true);

  connect(d->tree_view, &TreeExplorerTreeView::customContextMenuRequested,
          this, &TreeExplorer::OnOpenItemContextMenu);

  // Initialize the treeview item buttons
  d->treeview_item_buttons.open = new QPushButton(QIcon(), "", this);
  d->treeview_item_buttons.open->setToolTip(tr("Open"));

  connect(d->treeview_item_buttons.open, &QPushButton::pressed, this,
          &TreeExplorer::OnActivateTreeViewItem);

  d->treeview_item_buttons.expand = new QPushButton(QIcon(), "", this);
  d->treeview_item_buttons.expand->setToolTip(tr("Expand"));

  connect(d->treeview_item_buttons.expand, &QPushButton::pressed, this,
          &TreeExplorer::OnExpandTreeViewItem);

  d->treeview_item_buttons.goto_ = new QPushButton(QIcon(), "", this);
  d->treeview_item_buttons.goto_->setToolTip(tr("Goto original"));

  connect(d->treeview_item_buttons.goto_, &QPushButton::pressed, this,
          &TreeExplorer::OnGotoOriginalTreeViewItem);

  // Create the search widget
  d->search_widget = ISearchWidget::Create(ISearchWidget::Mode::Filter, this);
  connect(d->search_widget, &ISearchWidget::SearchParametersChanged, this,
          &TreeExplorer::OnSearchParametersChange);

  // Create the search widget addon
  d->filter_settings_widget = new FilterSettingsWidget(model, this);

  connect(d->search_widget, &ISearchWidget::Activated,
          d->filter_settings_widget, &FilterSettingsWidget::Activate);

  connect(d->search_widget, &ISearchWidget::Deactivated,
          d->filter_settings_widget, &FilterSettingsWidget::Deactivate);

  // Create the status widget
  d->status_widget = new QWidget(this);
  d->status_widget->setVisible(false);

  auto status_widget_layout = new QHBoxLayout();
  status_widget_layout->setContentsMargins(0, 0, 0, 0);

  status_widget_layout->addWidget(new QLabel(tr("Updating..."), this));
  status_widget_layout->addStretch();

  auto cancel_button = new QPushButton(tr("Cancel"), this);
  status_widget_layout->addWidget(cancel_button);

  connect(cancel_button, &QPushButton::pressed,
          model, &ITreeExplorerModel::CancelRunningRequest);

  connect(model, &ITreeExplorerModel::RequestStarted,
          this, &TreeExplorer::OnModelRequestStarted);

  connect(model, &ITreeExplorerModel::RequestFinished,
          this, &TreeExplorer::OnModelRequestFinished);

  d->status_widget->setLayout(status_widget_layout);

  // Setup the main layout
  setContentsMargins(0, 0, 0, 0);

  auto layout = new QVBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(d->tree_view);
  layout->addWidget(d->status_widget);
  layout->addWidget(d->filter_settings_widget);
  layout->addWidget(d->search_widget);
  setLayout(layout);

  // Setup che custom context menu
  d->context_menu.menu = new QMenu(tr("Entity tree browser menu"));
  d->context_menu.copy_details_action = new QAction(tr("Copy details"));

  d->context_menu.menu->addAction(d->context_menu.copy_details_action);

  connect(d->context_menu.menu, &QMenu::triggered, this,
          &TreeExplorer::OnContextMenuActionTriggered);

  connect(&IThemeManager::Get(), &IThemeManager::ThemeChanged, this,
          &TreeExplorer::OnThemeChange);

  OnThemeChange(IThemeManager::Get().GetPalette(),
                IThemeManager::Get().GetCodeViewTheme());
}

void TreeExplorer::InstallModel(ITreeExplorerModel *model,
                                IGlobalHighlighter *global_highlighter) {
  d->model = model;

  QAbstractItemModel *source_model{d->model};
  if (global_highlighter != nullptr) {
    source_model = global_highlighter->CreateModelProxy(
        source_model, ITreeExplorerModel::EntityIdRole);
  }

  d->model_proxy = new SearchFilterModelProxy(this);
  d->model_proxy->setRecursiveFilteringEnabled(true);
  d->model_proxy->setSourceModel(source_model);
  d->model_proxy->setDynamicSortFilter(true);

  connect(d->filter_settings_widget,
          &FilterSettingsWidget::ColumnFilterStateListChanged, d->model_proxy,
          &SearchFilterModelProxy::OnColumnFilterStateListChange);

  d->tree_view->setModel(d->model_proxy);

  // Note: this needs to happen after the model has been set in the
  // tree view!
  auto tree_selection_model = d->tree_view->selectionModel();
  connect(tree_selection_model, &QItemSelectionModel::currentChanged, this,
          &TreeExplorer::OnCurrentItemChanged);

  connect(d->model_proxy, &QAbstractItemModel::modelReset, this,
          &TreeExplorer::OnModelReset);

  connect(d->model_proxy, &QAbstractItemModel::dataChanged, this,
          &TreeExplorer::OnDataChanged);

  connect(d->model_proxy, &QAbstractItemModel::rowsInserted, this,
          &TreeExplorer::OnRowsInserted);

  OnModelReset();
}

ITreeExplorerModel *TreeExplorer::Model(void) const {
  return d->model;
}

void TreeExplorer::resizeEvent(QResizeEvent *) {
  UpdateTreeViewItemButtons();
}

void TreeExplorer::CopyTreeExplorerItemDetails(const QModelIndex &index) {
  auto tooltip_var = index.data(Qt::ToolTipRole);
  if (!tooltip_var.isValid()) {
    return;
  }

  const auto &tooltip = tooltip_var.toString();

  auto &clipboard = *QGuiApplication::clipboard();
  clipboard.setText(tooltip);
}

void TreeExplorer::ExpandTreeExplorerItem(const QModelIndex &index) {
  auto &model = *static_cast<TreeExplorerModel *>(d->model);
  model.ExpandEntity(d->model_proxy->mapToSource(index), 1u);
}

bool TreeExplorer::eventFilter(QObject *obj, QEvent *event) {
  if (obj == d->tree_view) {
    // Disable the overlay buttons while scrolling. It is hard to keep
    // them on screen due to how the scrolling event is propagated.
    if (event->type() == QEvent::Wheel) {
      auto scrolling_enabled =
          (d->tree_view->horizontalScrollBar()->isVisible() ||
           d->tree_view->verticalScrollBar()->isVisible());

      if (scrolling_enabled) {
        d->treeview_item_buttons.opt_hovered_index = std::nullopt;
        UpdateTreeViewItemButtons();
      }

      return false;

    } else {
      return false;
    }

  } else if (obj == d->tree_view->viewport()) {
    if (event->type() == QEvent::Leave || event->type() == QEvent::MouseMove) {
      // It is important to double check the leave event; it is sent
      // even if the mouse is still inside our treeview item but
      // above the hovering button (which steals the focus)
      auto mouse_pos = d->tree_view->viewport()->mapFromGlobal(QCursor::pos());

      auto index = d->tree_view->indexAt(mouse_pos);
      if (!index.isValid()) {
        d->treeview_item_buttons.opt_hovered_index = std::nullopt;
      } else {
        d->treeview_item_buttons.opt_hovered_index = index;
      }

      UpdateTreeViewItemButtons();
      return false;

    } else {
      return false;
    }

  } else {
    return false;
  }
}

void TreeExplorer::UpdateTreeViewItemButtons() {

  // TODO(pag): Sometimes we get infinite recursion from Qt doing a
  // `sendSyntheticEnterLeave` below when we `setVisible(is_redundant)`.
  if (d->treeview_item_buttons.updating_buttons) {
    return;
  }

  d->treeview_item_buttons.updating_buttons = true;
  d->treeview_item_buttons.open->setVisible(false);
  d->treeview_item_buttons.goto_->setVisible(false);
  d->treeview_item_buttons.expand->setVisible(false);

  // Always show the buttons, but disable the ones that are not
  // applicable. This is to prevent buttons from disappearing and/or
  // reordering while the user is clicking them
  auto display_buttons = d->treeview_item_buttons.opt_hovered_index.has_value();
  if (!display_buttons) {
    d->treeview_item_buttons.updating_buttons = false;
    return;
  }

  const auto &index = d->treeview_item_buttons.opt_hovered_index.value();

  // Enable the go-to button if we have a referenced entity id.
  d->treeview_item_buttons.open->setEnabled(false);
  d->treeview_item_buttons.expand->setEnabled(false);
  d->treeview_item_buttons.expand->setEnabled(false);

  auto entity_id_var = index.data(ITreeExplorerModel::EntityIdRole);
  if (entity_id_var.isValid() &&
      qvariant_cast<RawEntityId>(entity_id_var) != kInvalidEntityId) {
    d->treeview_item_buttons.open->setEnabled(true);
  }

  // Enable the expansion button if we haven't yet expanded the node.
  // TODO(alessandro): Fix the button visibility
  auto expand_var = index.data(ITreeExplorerModel::CanBeExpanded);
  d->treeview_item_buttons.expand->setEnabled(
      expand_var.isValid() && expand_var.toBool());

  // Show/hide one of expand/goto if this is redundant.
  auto redundant_var = index.data(ITreeExplorerModel::IsDuplicate);
  auto is_redundant = redundant_var.isValid() && redundant_var.toBool();

  d->treeview_item_buttons.open->setVisible(display_buttons);
  d->treeview_item_buttons.goto_->setVisible(is_redundant);
  d->treeview_item_buttons.expand->setVisible(!is_redundant);

  // Keep up to date with TreeviewItemButtons
  static constexpr auto kNumButtons = 2u;
  QPushButton *button_list[kNumButtons] = {
    d->treeview_item_buttons.open,
    (is_redundant ? d->treeview_item_buttons.goto_ :
     d->treeview_item_buttons.expand),
  };

  // Update the button positions
  auto rect = d->tree_view->visualRect(index);

  auto button_margin = rect.height() / 6;
  auto button_size = rect.height() - (button_margin * 2);
  auto button_count = static_cast<int>(kNumButtons);
  auto button_area_width =
      (button_count * button_size) + (button_count * button_margin);

  auto current_x =
      d->tree_view->pos().x() + d->tree_view->width() - button_area_width;

  const auto &vertical_scrollbar = *d->tree_view->verticalScrollBar();
  if (vertical_scrollbar.isVisible()) {
    current_x -= vertical_scrollbar.width();
  }

  auto current_y = rect.y() + (rect.height() / 2) - (button_size / 2);

  auto pos =
      d->tree_view->viewport()->mapToGlobal(QPoint(current_x, current_y));

  pos = mapFromGlobal(pos);

  current_x = pos.x();
  current_y = pos.y();

  for (auto *button : button_list) {
    button->resize(button_size, button_size);
    button->move(current_x, current_y);
    button->raise();

    current_x += button_size + button_margin;
  }

  d->treeview_item_buttons.updating_buttons = false;
}

void TreeExplorer::UpdateIcons() {
  QIcon open_item_icon;
  open_item_icon.addPixmap(GetPixmap(":/TreeExplorer/activate_ref_item"),
                           QIcon::Normal, QIcon::On);

  open_item_icon.addPixmap(
      GetPixmap(":/TreeExplorer/activate_ref_item", IconStyle::Disabled),
      QIcon::Disabled, QIcon::On);

  d->treeview_item_buttons.open->setIcon(open_item_icon);

  QIcon expand_item_icon;
  expand_item_icon.addPixmap(GetPixmap(":/TreeExplorer/expand_ref_item"),
                             QIcon::Normal, QIcon::On);

  expand_item_icon.addPixmap(
      GetPixmap(":/TreeExplorer/expand_ref_item", IconStyle::Disabled),
      QIcon::Disabled, QIcon::On);

  d->treeview_item_buttons.expand->setIcon(expand_item_icon);

  QIcon goto_item_icon;
  goto_item_icon.addPixmap(GetPixmap(":/TreeExplorer/goto_ref_item"),
                             QIcon::Normal, QIcon::On);

  goto_item_icon.addPixmap(
      GetPixmap(":/TreeExplorer/goto_ref_item", IconStyle::Disabled),
      QIcon::Disabled, QIcon::On);

  d->treeview_item_buttons.goto_->setIcon(goto_item_icon);
}

void TreeExplorer::OnModelReset() {
  ExpandAllNodes();
  d->treeview_item_buttons.opt_hovered_index = std::nullopt;
  UpdateTreeViewItemButtons();
}

void TreeExplorer::OnDataChanged() {
  UpdateTreeViewItemButtons();
  ExpandAllNodes();

  d->tree_view->viewport()->repaint();
}

void TreeExplorer::ExpandAllNodes() {
  d->tree_view->expandAll();
  d->tree_view->resizeColumnToContents(0);
}

void TreeExplorer::OnRowsInserted(const QModelIndex &parent, int, int) {
  d->tree_view->expandRecursively(parent);
  d->tree_view->resizeColumnToContents(0);
}

void TreeExplorer::OnCurrentItemChanged(const QModelIndex &current_index,
                                        const QModelIndex &) {

  if (!current_index.isValid()) {
    return;
  }

  emit SelectedItemChanged(current_index);
}

void TreeExplorer::OnOpenItemContextMenu(const QPoint &point) {
  auto index = d->tree_view->indexAt(point);
  if (!index.isValid()) {
    return;
  }

  QVariant action_data;
  action_data.setValue(index);

  for (auto &action : d->context_menu.menu->actions()) {
    action->setData(action_data);
  }

  auto menu_position = d->tree_view->viewport()->mapToGlobal(point);
  d->context_menu.menu->exec(menu_position);
}

void TreeExplorer::OnContextMenuActionTriggered(QAction *action) {
  auto index_var = action->data();
  if (!index_var.isValid()) {
    return;
  }

  const auto &index = qvariant_cast<QModelIndex>(index_var);
  if (!index.isValid()) {
    return;
  }

  if (action == d->context_menu.copy_details_action) {
    CopyTreeExplorerItemDetails(index);
  }
}

void TreeExplorer::OnSearchParametersChange(
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

  d->model_proxy->setFilterRegularExpression(regex);
  ExpandAllNodes();
}

void TreeExplorer::OnActivateTreeViewItem() {
  if (!d->treeview_item_buttons.opt_hovered_index.has_value()) {
    return;
  }

  const auto &index = d->treeview_item_buttons.opt_hovered_index.value();
  emit ItemActivated(index);
}

void TreeExplorer::OnExpandTreeViewItem() {
  if (!d->treeview_item_buttons.opt_hovered_index.has_value()) {
    return;
  }

  const auto &index = d->treeview_item_buttons.opt_hovered_index.value();
  ExpandTreeExplorerItem(index);
}

void TreeExplorer::OnGotoOriginalTreeViewItem(void) {
  if (!d->treeview_item_buttons.opt_hovered_index.has_value()) {
    return;
  }

  const auto &index = d->treeview_item_buttons.opt_hovered_index.value();
  (void) index;
}

void TreeExplorer::OnThemeChange(const QPalette &,
                                 const CodeViewTheme &code_view_theme) {
  UpdateIcons();
  InstallItemDelegate(code_view_theme);

  QFont font{code_view_theme.font_name};
  setFont(font);
}

void TreeExplorer::InstallItemDelegate(const CodeViewTheme &code_view_theme) {
  auto old_item_delegate = d->tree_view->itemDelegate();
  if (old_item_delegate != nullptr) {
    old_item_delegate->deleteLater();
  }

  auto tree_view_item_delegate =
      new TreeExplorerItemDelegate(code_view_theme, this);

  d->tree_view->setItemDelegate(tree_view_item_delegate);
}

void TreeExplorer::OnModelRequestStarted() {
  d->status_widget->setVisible(true);
  d->model_proxy->setDynamicSortFilter(false);
}

void TreeExplorer::OnModelRequestFinished() {
  d->status_widget->setVisible(false);
  d->model_proxy->setDynamicSortFilter(true);
}

}  // namespace mx::gui
