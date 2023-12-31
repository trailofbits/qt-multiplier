/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/GUI/Widgets/TreeGeneratorWidget.h>

#include <QAction>
#include <QClipboard>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QScrollBar>

#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/MediaManager.h>
#include <multiplier/GUI/Widgets/FilterSettingsWidget.h>
#include <multiplier/GUI/Widgets/SearchWidget.h>
#include <multiplier/GUI/Widgets/TreeWidget.h>

#include "SearchFilterModelProxy.h"
#include "TreeGeneratorModel.h"

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

struct TreeGeneratorWidget::PrivateData final {
  TreeGeneratorModel *model{nullptr};
  SearchFilterModelProxy *model_proxy{nullptr};

  TreeWidget *tree_widget{nullptr};
  SearchWidget *search_widget{nullptr};
  FilterSettingsWidget *filter_settings_widget{nullptr};
  ContextMenu context_menu;
  QWidget *status_widget{nullptr};
  TreeviewItemButtons tree_item_buttons;
};

TreeGeneratorWidget::~TreeGeneratorWidget(void) {}

TreeGeneratorWidget::TreeGeneratorWidget(
    const ConfigManager &config_manager, ITreeGeneratorPtr generator,
    QWidget *parent)
    : QWidget(parent),
      d(new PrivateData) {

  auto model = new TreeGeneratorModel(this);
  InitializeWidgets(config_manager, model);

  model->InstallGenerator(std::move(generator));
  InstallModel(model);

  // Synchronize the search widget and its addon
  d->search_widget->Deactivate();
}

void TreeGeneratorWidget::InstallModel(TreeGeneratorModel *model) {
  d->model = model;

  d->model_proxy = new SearchFilterModelProxy(this);
  d->model_proxy->setRecursiveFilteringEnabled(true);
  d->model_proxy->setSourceModel(model);
  d->model_proxy->setDynamicSortFilter(true);

  connect(d->filter_settings_widget,
          &FilterSettingsWidget::ColumnFilterStateListChanged,
          d->model_proxy,
          &SearchFilterModelProxy::OnColumnFilterStateListChange);

  d->tree_widget->setModel(d->model_proxy);

  // Note: this needs to happen after the model has been set in the
  // tree view!
  auto tree_selection_model = d->tree_widget->selectionModel();
  connect(tree_selection_model, &QItemSelectionModel::currentChanged,
          this, &TreeGeneratorWidget::OnCurrentItemChanged);

  connect(d->model_proxy, &QAbstractItemModel::modelReset,
          this, &TreeGeneratorWidget::OnModelReset);

  connect(d->model_proxy, &QAbstractItemModel::dataChanged,
          this, &TreeGeneratorWidget::OnDataChanged);

  connect(d->model_proxy, &QAbstractItemModel::rowsInserted,
          this, &TreeGeneratorWidget::OnRowsInserted);

  OnModelReset();
}

void TreeGeneratorWidget::InitializeWidgets(
    const ConfigManager &config_manager, TreeGeneratorModel *model) {

  auto &theme_manager = config_manager.ThemeManager();
  auto &media_manager = config_manager.MediaManager();

  // Initialize the tree view
  d->tree_widget = new TreeWidget(this);
  d->tree_widget->setSortingEnabled(true);
  d->tree_widget->sortByColumn(0, Qt::AscendingOrder);

  // The auto scroll takes care of keeping the active item within the
  // visible viewport region. This is true for mouse clicks but also
  // keyboard navigation (i.e. arrow keys, page up/down, etc).
  d->tree_widget->setAutoScroll(false);

  // Smooth scrolling.
  d->tree_widget->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  d->tree_widget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

  // We'll potentially have a bunch of columns depending on the configuration,
  // so make sure they span to use all available space.
  QHeaderView *header = d->tree_widget->header();
  header->setStretchLastSection(true);

  // Don't let double click expand things in three; we capture double click so
  // that we can make it open up the use in the code.
  d->tree_widget->setExpandsOnDoubleClick(false);

  // Disallow multiple selection. If we have grouping by file enabled, then when
  // a user clicks on a file name, we instead jump down to the first entry
  // grouped under that file. This is to make using the up/down arrows easier.
  d->tree_widget->setSelectionBehavior(QAbstractItemView::SelectRows);
  d->tree_widget->setSelectionMode(QAbstractItemView::SingleSelection);
  d->tree_widget->setAllColumnsShowFocus(true);
  d->tree_widget->setTreePosition(0);
  d->tree_widget->setTextElideMode(Qt::TextElideMode::ElideRight);

  d->tree_widget->setAlternatingRowColors(false);
  d->tree_widget->setContextMenuPolicy(Qt::CustomContextMenu);
  d->tree_widget->installEventFilter(this);
  d->tree_widget->viewport()->installEventFilter(this);
  d->tree_widget->viewport()->setMouseTracking(true);

  connect(d->tree_widget, &TreeWidget::customContextMenuRequested,
          this, &TreeGeneratorWidget::OnOpenItemContextMenu);

  // Initialize the treeview item buttons
  d->tree_item_buttons.open = new QPushButton(QIcon(), "", this);
  d->tree_item_buttons.open->setToolTip(tr("Open"));

  connect(d->tree_item_buttons.open, &QPushButton::pressed, this,
          &TreeGeneratorWidget::OnActivateItem);

  d->tree_item_buttons.expand = new QPushButton(QIcon(), "", this);
  d->tree_item_buttons.expand->setToolTip(tr("Expand"));

  connect(d->tree_item_buttons.expand, &QPushButton::pressed, this,
          &TreeGeneratorWidget::OnExpandItem);

  d->tree_item_buttons.goto_ = new QPushButton(QIcon(), "", this);
  d->tree_item_buttons.goto_->setToolTip(tr("Goto original"));

  connect(d->tree_item_buttons.goto_, &QPushButton::pressed, this,
          &TreeGeneratorWidget::OnGotoOriginalItem);

  // Create the search widget
  d->search_widget = new SearchWidget(
      media_manager, SearchWidget::Mode::Filter, this);

  connect(d->search_widget, &SearchWidget::SearchParametersChanged,
          this, &TreeGeneratorWidget::OnSearchParametersChange);

  // Create the search widget addon
  d->filter_settings_widget = new FilterSettingsWidget(model, this);

  connect(d->search_widget, &SearchWidget::Activated,
          d->filter_settings_widget, &FilterSettingsWidget::Activate);

  connect(d->search_widget, &SearchWidget::Deactivated,
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
          model, &TreeGeneratorModel::CancelRunningRequest);

  connect(model, &TreeGeneratorModel::RequestStarted,
          this, &TreeGeneratorWidget::OnModelRequestStarted);

  connect(model, &TreeGeneratorModel::RequestFinished,
          this, &TreeGeneratorWidget::OnModelRequestFinished);

  d->status_widget->setLayout(status_widget_layout);

  // Setup the main layout
  setContentsMargins(0, 0, 0, 0);

  auto layout = new QVBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(d->tree_widget);
  layout->addWidget(d->status_widget);
  layout->addWidget(d->filter_settings_widget);
  layout->addWidget(d->search_widget);
  setLayout(layout);

  // Setup che custom context menu
  d->context_menu.menu = new QMenu(tr("Entity tree browser menu"));
  d->context_menu.copy_details_action = new QAction(tr("Copy details"));

  d->context_menu.menu->addAction(d->context_menu.copy_details_action);

  connect(d->context_menu.menu, &QMenu::triggered,
          this, &TreeGeneratorWidget::OnContextMenuActionTriggered);

  // Set the theme.
  connect(&theme_manager, &ThemeManager::ThemeChanged,
          this, &TreeGeneratorWidget::OnThemeChanged);

  OnThemeChanged(theme_manager);

  // Set the icons.
  connect(&media_manager, &MediaManager::IconsChanged,
          this, &TreeGeneratorWidget::OnIconsChanged);

  OnIconsChanged(media_manager);

  config_manager.InstallItemDelegate(d->tree_widget);
}

void TreeGeneratorWidget::CopyItemDetails(
    const QModelIndex &index) {
  auto tooltip_var = index.data(Qt::ToolTipRole);
  if (!tooltip_var.isValid()) {
    return;
  }

  const auto &tooltip = tooltip_var.toString();

  auto &clipboard = *QGuiApplication::clipboard();
  clipboard.setText(tooltip);
}

void TreeGeneratorWidget::ExpandItem(
    const QModelIndex &index, unsigned depth) {
  auto source_index = d->model_proxy->mapToSource(index);
  d->model->Expand(source_index, depth);
}

//! Called when attempt to go to the original verison of a duplicate item.
void TreeGeneratorWidget::GotoOriginalItem(const QModelIndex &index) {
  auto orig = d->model_proxy->mapToSource(index);
  auto dedup = d->model->Deduplicate(orig);
  dedup = d->model_proxy->mapFromSource(dedup);
  if (!dedup.isValid()) {
    return;
  }

  auto sel = d->tree_widget->selectionModel();
  sel->clearSelection();
  sel->setCurrentIndex(dedup, QItemSelectionModel::Select);
  d->tree_widget->scrollTo(dedup);
}

bool TreeGeneratorWidget::eventFilter(QObject *obj, QEvent *event) {
  if (obj == d->tree_widget) {
    // Disable the overlay buttons while scrolling. It is hard to keep
    // them on screen due to how the scrolling event is propagated.
    if (event->type() == QEvent::Wheel) {
      auto scrolling_enabled =
          (d->tree_widget->horizontalScrollBar()->isVisible() ||
           d->tree_widget->verticalScrollBar()->isVisible());

      if (scrolling_enabled) {
        d->tree_item_buttons.opt_hovered_index = std::nullopt;
        UpdateItemButtons();
      }

      return false;

    } else if (event->type() != QEvent::KeyRelease) {
      return false;

    } else if (QKeyEvent *kevent = dynamic_cast<QKeyEvent *>(event)) {
      auto ret = false;
      for (auto index : d->tree_widget->selectionModel()->selectedIndexes()) {
        switch (kevent->key()) {
          case Qt::Key_1:
          case Qt::Key_2:
          case Qt::Key_3:
          case Qt::Key_4:
          case Qt::Key_5:
          case Qt::Key_6:
          case Qt::Key_7:
          case Qt::Key_8:
          case Qt::Key_9:
            ExpandItem(index, static_cast<unsigned>(kevent->key() - Qt::Key_0));
            ret = true;
            break;
          default: break;
        }
      }

      return ret;
    }

    return false;

  } else if (obj == d->tree_widget->viewport()) {
    if (event->type() == QEvent::Leave || event->type() == QEvent::MouseMove) {
      // It is important to double check the leave event; it is sent
      // even if the mouse is still inside our treeview item but
      // above the hovering button (which steals the focus)
      auto mouse_pos = d->tree_widget->viewport()->mapFromGlobal(QCursor::pos());

      auto index = d->tree_widget->indexAt(mouse_pos);
      if (!index.isValid()) {
        d->tree_item_buttons.opt_hovered_index = std::nullopt;
      } else {
        d->tree_item_buttons.opt_hovered_index = index;
      }

      UpdateItemButtons();
      return false;

    } else {
      return false;
    }

  } else {
    return false;
  }
}

void TreeGeneratorWidget::resizeEvent(QResizeEvent *) {
  UpdateItemButtons();
}

void TreeGeneratorWidget::UpdateItemButtons(void) {

  // TODO(pag): Sometimes we get infinite recursion from Qt doing a
  // `sendSyntheticEnterLeave` below when we `setVisible(is_redundant)`.
  if (d->tree_item_buttons.updating_buttons) {
    return;
  }

  d->tree_item_buttons.updating_buttons = true;
  d->tree_item_buttons.open->setVisible(false);
  d->tree_item_buttons.goto_->setVisible(false);
  d->tree_item_buttons.expand->setVisible(false);

  // Always show the buttons, but disable the ones that are not
  // applicable. This is to prevent buttons from disappearing and/or
  // reordering while the user is clicking them
  auto display_buttons = d->tree_item_buttons.opt_hovered_index.has_value();
  if (!display_buttons) {
    d->tree_item_buttons.updating_buttons = false;
    return;
  }

  const auto &index = d->tree_item_buttons.opt_hovered_index.value();

  // Enable the go-to button if we have a referenced entity id.
  d->tree_item_buttons.open->setEnabled(false);
  d->tree_item_buttons.expand->setEnabled(false);
  d->tree_item_buttons.expand->setEnabled(false);

  auto entity_id_var = index.data(TreeGeneratorModel::EntityIdRole);
  if (entity_id_var.isValid() &&
      qvariant_cast<RawEntityId>(entity_id_var) != kInvalidEntityId) {
    d->tree_item_buttons.open->setEnabled(true);
  }

  // Enable the expansion button if we haven't yet expanded the node.
  // TODO(alessandro): Fix the button visibility
  auto expand_var = index.data(TreeGeneratorModel::CanBeExpanded);
  d->tree_item_buttons.expand->setEnabled(expand_var.isValid() &&
                                          expand_var.toBool());

  // Show/hide one of expand/goto if this is redundant.
  auto redundant_var = index.data(TreeGeneratorModel::IsDuplicate);
  auto is_redundant = redundant_var.isValid() && redundant_var.toBool();

  d->tree_item_buttons.open->setVisible(display_buttons);
  d->tree_item_buttons.goto_->setVisible(is_redundant);
  d->tree_item_buttons.expand->setVisible(!is_redundant);

  // Keep up to date with TreeviewItemButtons
  static constexpr auto kNumButtons = 2u;
  QPushButton *button_list[kNumButtons] = {
      d->tree_item_buttons.open,
      (is_redundant ? d->tree_item_buttons.goto_
                    : d->tree_item_buttons.expand),
  };

  // Update the button positions
  auto rect = d->tree_widget->visualRect(index);

  auto button_margin = rect.height() / 6;
  auto button_size = rect.height() - (button_margin * 2);
  auto button_count = static_cast<int>(kNumButtons);
  auto button_area_width =
      (button_count * button_size) + (button_count * button_margin);

  auto current_x =
      d->tree_widget->pos().x() + d->tree_widget->width() - button_area_width;

  const auto &vertical_scrollbar = *d->tree_widget->verticalScrollBar();
  if (vertical_scrollbar.isVisible()) {
    current_x -= vertical_scrollbar.width();
  }

  auto current_y = rect.y() + (rect.height() / 2) - (button_size / 2);

  auto pos =
      d->tree_widget->viewport()->mapToGlobal(QPoint(current_x, current_y));

  pos = mapFromGlobal(pos);

  current_x = pos.x();
  current_y = pos.y();

  for (auto *button : button_list) {
    button->resize(button_size, button_size);
    button->move(current_x, current_y);
    button->raise();

    current_x += button_size + button_margin;
  }

  d->tree_item_buttons.updating_buttons = false;
}

void TreeGeneratorWidget::OnIconsChanged(const MediaManager &media_manager) {
  QIcon open_item_icon;
  open_item_icon.addPixmap(
      media_manager.Pixmap("com.trailofbits.icon.Activate"),
      QIcon::Normal, QIcon::On);

  open_item_icon.addPixmap(
      media_manager.Pixmap("com.trailofbits.icon.Activate",
                           ITheme::IconStyle::DISABLED),
      QIcon::Disabled, QIcon::On);

  d->tree_item_buttons.open->setIcon(open_item_icon);

  QIcon expand_item_icon;
  expand_item_icon.addPixmap(
      media_manager.Pixmap("com.trailofbits.icon.Expand"),
      QIcon::Normal, QIcon::On);

  expand_item_icon.addPixmap(
      media_manager.Pixmap("com.trailofbits.icon.Expand",
                           ITheme::IconStyle::DISABLED),
      QIcon::Disabled, QIcon::On);

  d->tree_item_buttons.expand->setIcon(expand_item_icon);

  QIcon goto_item_icon;
  goto_item_icon.addPixmap(
      media_manager.Pixmap("com.trailofbits.icon.Goto"),
      QIcon::Normal, QIcon::On);

  goto_item_icon.addPixmap(
      media_manager.Pixmap("com.trailofbits.icon.Goto",
                           ITheme::IconStyle::DISABLED),
      QIcon::Disabled, QIcon::On);

  d->tree_item_buttons.goto_->setIcon(goto_item_icon);
}

void TreeGeneratorWidget::OnModelReset(void) {
  ExpandAllNodes();
  d->tree_item_buttons.opt_hovered_index = std::nullopt;
  UpdateItemButtons();
}

void TreeGeneratorWidget::OnDataChanged(void) {
  UpdateItemButtons();
  ExpandAllNodes();

  d->tree_widget->viewport()->repaint();
}

void TreeGeneratorWidget::ExpandAllNodes(void) {
  d->tree_widget->expandAll();
  d->tree_widget->resizeColumnToContents(0);
}

void TreeGeneratorWidget::OnRowsInserted(const QModelIndex &parent, int, int) {
  d->tree_widget->expandRecursively(parent);
  d->tree_widget->resizeColumnToContents(0);
}

void TreeGeneratorWidget::OnCurrentItemChanged(const QModelIndex &current_index,
                                        const QModelIndex &) {

  if (!current_index.isValid()) {
    return;
  }

  emit SelectedItemChanged(current_index);
}

void TreeGeneratorWidget::OnOpenItemContextMenu(const QPoint &point) {
  auto index = d->tree_widget->indexAt(point);
  if (!index.isValid()) {
    return;
  }

  QVariant action_data;
  action_data.setValue(index);

  for (auto &action : d->context_menu.menu->actions()) {
    action->setData(action_data);
  }

  auto menu_position = d->tree_widget->viewport()->mapToGlobal(point);
  d->context_menu.menu->exec(menu_position);
}

void TreeGeneratorWidget::OnContextMenuActionTriggered(QAction *action) {
  auto index_var = action->data();
  if (!index_var.isValid()) {
    return;
  }

  const auto &index = qvariant_cast<QModelIndex>(index_var);
  if (!index.isValid()) {
    return;
  }

  if (action == d->context_menu.copy_details_action) {
    CopyItemDetails(index);
  }
}

void TreeGeneratorWidget::OnSearchParametersChange(void) {
  const auto &search_parameters = d->search_widget->Parameters();
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

  d->model_proxy->setFilterRegularExpression(regex);
  ExpandAllNodes();
}

void TreeGeneratorWidget::OnActivateItem(void) {
  if (!d->tree_item_buttons.opt_hovered_index.has_value()) {
    return;
  }

  const auto &index = d->tree_item_buttons.opt_hovered_index.value();
  emit ItemActivated(index);
}

void TreeGeneratorWidget::OnExpandItem(void) {
  if (!d->tree_item_buttons.opt_hovered_index.has_value()) {
    return;
  }

  ExpandItem(d->tree_item_buttons.opt_hovered_index.value());
}

void TreeGeneratorWidget::GotoOriginalItem(void) {
  if (!d->tree_item_buttons.opt_hovered_index.has_value()) {
    return;
  }

  ExpandItem(d->tree_item_buttons.opt_hovered_index.value());
}

// NOTE(pag): The config manager handles the item delegate automatically.
void TreeGeneratorWidget::OnThemeChanged(const ThemeManager &theme_manager) {
  setFont(theme_manager.Theme()->Font());
  d->model->OnThemeChanged(theme_manager);
}

void TreeGeneratorWidget::OnModelRequestStarted(void) {
  d->status_widget->setVisible(true);
  d->model_proxy->setDynamicSortFilter(false);
}

void TreeGeneratorWidget::OnModelRequestFinished(void) {
  d->status_widget->setVisible(false);
  d->model_proxy->setDynamicSortFilter(true);
}

}  // namespace mx::gui