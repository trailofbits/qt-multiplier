/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "GraphicalReferenceExplorer.h"
#include "FilterSettingsWidget.h"
#include "ReferenceExplorerItemDelegate.h"
#include "ReferenceExplorerModel.h"
#include "ReferenceExplorerTreeView.h"
#include "SearchFilterModelProxy.h"

#include <multiplier/ui/Assert.h>
#include <multiplier/ui/Icons.h>

#include <QApplication>
#include <QClipboard>
#include <QDropEvent>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QVBoxLayout>
#include <QScrollBar>

#include <optional>

namespace mx::gui {

namespace {

struct ContextMenu final {
  QMenu *menu{nullptr};
  QAction *copy_details_action{nullptr};
  QAction *set_root_action{nullptr};
};

struct TreeviewItemButtons final {
  std::optional<QModelIndex> opt_hovered_index;

  // Keep up to date with UpdateTreeViewItemButtons
  QPushButton *open{nullptr};
  QPushButton *close{nullptr};
  QPushButton *expand{nullptr};
};

}  // namespace

struct GraphicalReferenceExplorer::PrivateData final {
  IReferenceExplorerModel *model{nullptr};
  SearchFilterModelProxy *model_proxy{nullptr};
  QTreeView *tree_view{nullptr};
  ISearchWidget *search_widget{nullptr};
  FilterSettingsWidget *filter_settings_widget{nullptr};
  QWidget *alternative_root_warning{nullptr};
  ContextMenu context_menu;

  TreeviewItemButtons treeview_item_buttons;
};

GraphicalReferenceExplorer::~GraphicalReferenceExplorer() {}

IReferenceExplorerModel *GraphicalReferenceExplorer::Model() {
  return d->model;
}

void GraphicalReferenceExplorer::resizeEvent(QResizeEvent *) {
  UpdateTreeViewItemButtons();
}

GraphicalReferenceExplorer::GraphicalReferenceExplorer(
    IReferenceExplorerModel *model, QWidget *parent,
    IGlobalHighlighter *global_highlighter)
    : IReferenceExplorer(parent),
      d(new PrivateData) {

  InitializeWidgets();
  InstallModel(model, global_highlighter);

  // Synchronize the search widget and its addon
  d->search_widget->Deactivate();
}

void GraphicalReferenceExplorer::InitializeWidgets() {
  // Initialize the tree view
  d->tree_view = new QTreeView(this);

  // TODO(pag): Re-enable with some kind of "intrusive" sort that makes the
  //            dropping of dragged items disable sort by encoding the current
  //            sort order into the model by re-ordering node children, then
  //            set the sort to a NOP sort based on this model data, that way
  //            when we drop things, they will land where they were dropped.
  d->tree_view->setSortingEnabled(true);
  d->tree_view->sortByColumn(0, Qt::AscendingOrder);

  // The auto scroll takes care of keeping the active item within the
  // visible viewport region. This is true for mouse clicks but also
  // keyboard navigation (i.e. arrow keys, page up/down, etc).
  //
  // Commenting out the following code for now:
  // d->tree_view->setAutoScroll(false);

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
  d->tree_view->setItemDelegateForColumn(0, new ReferenceExplorerItemDelegate);
  d->tree_view->setContextMenuPolicy(Qt::CustomContextMenu);
  d->tree_view->installEventFilter(this);
  d->tree_view->viewport()->installEventFilter(this);
  d->tree_view->viewport()->setMouseTracking(true);



  connect(d->tree_view, &ReferenceExplorerTreeView::customContextMenuRequested,
          this, &GraphicalReferenceExplorer::OnOpenItemContextMenu);

  QIcon open_item_icon;
  open_item_icon.addPixmap(GetPixmap(":/ReferenceExplorer/activate_ref_item"),
                           QIcon::Normal, QIcon::On);

  open_item_icon.addPixmap(
      GetPixmap(":/ReferenceExplorer/activate_ref_item", IconStyle::Disabled),
      QIcon::Disabled, QIcon::On);

  QIcon expand_item_icon;
  expand_item_icon.addPixmap(GetPixmap(":/ReferenceExplorer/expand_ref_item"),
                             QIcon::Normal, QIcon::On);

  expand_item_icon.addPixmap(
      GetPixmap(":/ReferenceExplorer/expand_ref_item", IconStyle::Disabled),
      QIcon::Disabled, QIcon::On);

  // Initialize the treeview item buttons
  d->treeview_item_buttons.open = new QPushButton(open_item_icon, "", this);
  d->treeview_item_buttons.open->setToolTip(tr("Open"));

  connect(d->treeview_item_buttons.open, &QPushButton::pressed, this,
          &GraphicalReferenceExplorer::OnActivateTreeViewItem);

  d->treeview_item_buttons.close =
      new QPushButton(GetIcon(":/ReferenceExplorer/close_ref_item"), "", this);

  d->treeview_item_buttons.close->setToolTip(tr("Close"));

  connect(d->treeview_item_buttons.close, &QPushButton::pressed, this,
          &GraphicalReferenceExplorer::OnCloseTreeViewItem);

  d->treeview_item_buttons.expand = new QPushButton(expand_item_icon, "", this);
  d->treeview_item_buttons.expand->setToolTip(tr("Expand"));

  connect(d->treeview_item_buttons.expand, &QPushButton::pressed, this,
          &GraphicalReferenceExplorer::OnExpandTreeViewItem);

  // Create the search widget
  d->search_widget = ISearchWidget::Create(ISearchWidget::Mode::Filter, this);
  connect(d->search_widget, &ISearchWidget::SearchParametersChanged, this,
          &GraphicalReferenceExplorer::OnSearchParametersChange);

  // Create the search widget addon
  d->filter_settings_widget = new FilterSettingsWidget(this);

  connect(d->filter_settings_widget,
          &FilterSettingsWidget::FilterParametersChanged, this,
          &GraphicalReferenceExplorer::OnFilterParametersChange);

  connect(d->search_widget, &ISearchWidget::Activated,
          d->filter_settings_widget, &FilterSettingsWidget::Activate);

  connect(d->search_widget, &ISearchWidget::Deactivated,
          d->filter_settings_widget, &FilterSettingsWidget::Deactivate);

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
          &GraphicalReferenceExplorer::OnDisableCustomRootLinkClicked);

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
  layout->addWidget(d->filter_settings_widget);
  layout->addWidget(d->search_widget);
  layout->addWidget(d->alternative_root_warning);
  setLayout(layout);

  // Setup che custom context menu
  d->context_menu.menu = new QMenu(tr("Reference browser menu"));
  d->context_menu.copy_details_action = new QAction(tr("Copy details"));
  d->context_menu.set_root_action = new QAction(tr("Set as root"));

  d->context_menu.menu->addAction(d->context_menu.copy_details_action);
  d->context_menu.menu->addSeparator();
  d->context_menu.menu->addAction(d->context_menu.set_root_action);

  connect(d->context_menu.menu, &QMenu::triggered, this,
          &GraphicalReferenceExplorer::OnContextMenuActionTriggered);
}

void GraphicalReferenceExplorer::InstallModel(
    IReferenceExplorerModel *model, IGlobalHighlighter *global_highlighter) {
  d->model = model;

  QAbstractItemModel *source_model{d->model};
  if (global_highlighter != nullptr) {
    source_model = global_highlighter->CreateModelProxy(
        source_model, IReferenceExplorerModel::EntityIdRole);
  }

  d->model_proxy = new SearchFilterModelProxy(this);
  d->model_proxy->setRecursiveFilteringEnabled(true);
  d->model_proxy->setSourceModel(source_model);

  d->tree_view->setModel(d->model_proxy);

  // Note: this needs to happen after the model has been set in the
  // tree view!
  auto tree_selection_model = d->tree_view->selectionModel();
  connect(tree_selection_model, &QItemSelectionModel::currentChanged, this,
          &GraphicalReferenceExplorer::OnCurrentItemChanged);

  connect(d->model_proxy, &QAbstractItemModel::modelReset, this,
          &GraphicalReferenceExplorer::OnModelReset);

  connect(d->model_proxy, &QAbstractItemModel::dataChanged, this,
          &GraphicalReferenceExplorer::OnDataChanged);

  connect(d->model_proxy, &QAbstractItemModel::rowsInserted, this,
          &GraphicalReferenceExplorer::OnRowsInserted);

  OnModelReset();
}

void GraphicalReferenceExplorer::CopyRefExplorerItemDetails(
    const QModelIndex &index) {
  auto tooltip_var = index.data(Qt::ToolTipRole);
  if (!tooltip_var.isValid()) {
    return;
  }

  const auto &tooltip = tooltip_var.toString();

  auto &clipboard = *QGuiApplication::clipboard();
  clipboard.setText(tooltip);
}

void GraphicalReferenceExplorer::RemoveRefExplorerItem(
    const QModelIndex &index) {
  d->model->RemoveEntity(d->model_proxy->mapToSource(index));
}

void GraphicalReferenceExplorer::ExpandRefExplorerItem(
    const QModelIndex &index) {
  d->model->ExpandEntity(d->model_proxy->mapToSource(index));
}

bool GraphicalReferenceExplorer::eventFilter(QObject *obj, QEvent *event) {
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

void GraphicalReferenceExplorer::UpdateTreeViewItemButtons() {
  // Keep up to date with TreeviewItemButtons
  std::vector<QPushButton *> button_list{d->treeview_item_buttons.open,
                                         d->treeview_item_buttons.close,
                                         d->treeview_item_buttons.expand};

  // Always show the buttons, but disable the ones that are not
  // applicable. This is to prevent buttons from disappearing and/or
  // reordering while the user is clicking them
  auto display_buttons = d->treeview_item_buttons.opt_hovered_index.has_value();

  for (auto &button : button_list) {
    button->setVisible(display_buttons);
  }

  if (!display_buttons) {
    return;
  }

  const auto &index = d->treeview_item_buttons.opt_hovered_index.value();

  // Enable the go-to button if we have a referenced entity id.
  d->treeview_item_buttons.open->setEnabled(false);
  d->treeview_item_buttons.expand->setEnabled(false);

  auto entity_id_var =
      index.data(IReferenceExplorerModel::ReferencedEntityIdRole);

  if (entity_id_var.isValid() &&
      qvariant_cast<RawEntityId>(entity_id_var) != kInvalidEntityId) {

    d->treeview_item_buttons.open->setEnabled(true);
  }

  // Enable the expansion button if we haven't yet expanded the node.
  auto expansion_status_var =
      index.data(IReferenceExplorerModel::ExpansionStatusRole);

  if (expansion_status_var.isValid() && !expansion_status_var.toBool()) {
    d->treeview_item_buttons.expand->setEnabled(true);
  }

  // Update the button positions
  auto rect = d->tree_view->visualRect(index);

  auto button_margin = rect.height() / 6;
  auto button_size = rect.height() - (button_margin * 2);

  auto button_count = static_cast<int>(button_list.size());
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
}

void GraphicalReferenceExplorer::OnModelReset() {
  auto display_root_warning = d->model->HasAlternativeRoot();
  d->alternative_root_warning->setVisible(display_root_warning);

  ExpandAllNodes();

  d->treeview_item_buttons.opt_hovered_index = std::nullopt;
  UpdateTreeViewItemButtons();
}

void GraphicalReferenceExplorer::OnDataChanged() {
  UpdateTreeViewItemButtons();
  ExpandAllNodes();

  d->tree_view->viewport()->repaint();
}

void GraphicalReferenceExplorer::ExpandAllNodes() {
  d->tree_view->expandAll();

  d->tree_view->resizeColumnToContents(0);
  d->tree_view->resizeColumnToContents(1);
}

void GraphicalReferenceExplorer::OnRowsInserted(const QModelIndex &, int, int) {
  ExpandAllNodes();
}

void GraphicalReferenceExplorer::OnCurrentItemChanged(
    const QModelIndex &current_index, const QModelIndex &) {

  if (!current_index.isValid()) {
    return;
  }

  emit SelectedItemChanged(current_index);
}

void GraphicalReferenceExplorer::OnOpenItemContextMenu(const QPoint &point) {
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

void GraphicalReferenceExplorer::OnContextMenuActionTriggered(QAction *action) {
  auto index_var = action->data();
  if (!index_var.isValid()) {
    return;
  }

  const auto &index = qvariant_cast<QModelIndex>(index_var);
  if (!index.isValid()) {
    return;
  }

  if (action == d->context_menu.copy_details_action) {
    CopyRefExplorerItemDetails(index);

  } else if (action == d->context_menu.set_root_action) {
    d->model->SetRoot(index);
  }
}

void GraphicalReferenceExplorer::OnSearchParametersChange(
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

  d->tree_view->expandRecursively(QModelIndex());
  d->tree_view->resizeColumnToContents(0);
}

void GraphicalReferenceExplorer::OnFilterParametersChange() {
  d->model_proxy->EnableFileNameFilter(
      d->filter_settings_widget->FilterByFileName());

  d->model_proxy->EnableEntityNameFilter(
      d->filter_settings_widget->FilterByEntityName());

  d->model_proxy->EnableBreadcrumbsFilter(
      d->filter_settings_widget->FilterByBreadcrumbs());


  d->model_proxy->EnableEntityIDFilter(
      d->filter_settings_widget->FilterByEntityID());
}

void GraphicalReferenceExplorer::OnDisableCustomRootLinkClicked() {
  d->model->SetDefaultRoot();
}

void GraphicalReferenceExplorer::OnActivateTreeViewItem() {
  if (!d->treeview_item_buttons.opt_hovered_index.has_value()) {
    return;
  }

  const auto &index = d->treeview_item_buttons.opt_hovered_index.value();
  emit ItemActivated(index);
}

void GraphicalReferenceExplorer::OnCloseTreeViewItem() {
  if (!d->treeview_item_buttons.opt_hovered_index.has_value()) {
    return;
  }

  const auto &index = d->treeview_item_buttons.opt_hovered_index.value();
  RemoveRefExplorerItem(index);
}

void GraphicalReferenceExplorer::OnExpandTreeViewItem() {
  if (!d->treeview_item_buttons.opt_hovered_index.has_value()) {
    return;
  }

  const auto &index = d->treeview_item_buttons.opt_hovered_index.value();
  ExpandRefExplorerItem(index);
}

}  // namespace mx::gui
