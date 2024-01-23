/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/GUI/Widgets/TreeGeneratorWidget.h>

#include <QAction>
#include <QClipboard>
#include <QElapsedTimer>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QScrollBar>
#include <QTreeView>

// #include <QAbstractItemModelTester>

#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/MediaManager.h>
#include <multiplier/GUI/Widgets/FilterSettingsWidget.h>
#include <multiplier/GUI/Widgets/SearchWidget.h>

#include "SearchFilterModelProxy.h"
#include "TreeGeneratorModel.h"

namespace mx::gui {
namespace {

static constexpr unsigned kMaxExpansionLevel = 9u;

}  // namespace

struct TreeGeneratorWidget::PrivateData final {
  TreeGeneratorModel *model{nullptr};
  SearchFilterModelProxy *model_proxy{nullptr};

  QTreeView *tree_view{nullptr};
  SearchWidget *search_widget{nullptr};
  FilterSettingsWidget *filter_settings_widget{nullptr};
  QWidget *status_widget{nullptr};

  bool updating_buttons{false};

  // Keep up to date with UpdateTreeViewItemButtons
  QPushButton *open{nullptr};
  QPushButton *expand{nullptr};
  QPushButton *goto_{nullptr};

  QIcon open_item_icon;
  QIcon expand_item_icon;
  QIcon goto_item_icon;
  QIcon expand_item_icon_n[kMaxExpansionLevel];

  QModelIndex selected_index;
  QElapsedTimer selection_timer;
};

TreeGeneratorWidget::~TreeGeneratorWidget(void) {}

TreeGeneratorWidget::TreeGeneratorWidget(
    const ConfigManager &config_manager, QWidget *parent)
    : IWindowWidget(parent),
      d(new PrivateData) {

  d->selection_timer.start();
  d->model = new TreeGeneratorModel(this);

  // (void) new QAbstractItemModelTester(
  //     d->model, QAbstractItemModelTester::FailureReportingMode::Fatal, this);

  InitializeWidgets(config_manager);
  InstallModel();

  // Synchronize the search widget and its addon
  d->search_widget->Deactivate();
}

//! Install a new generator.
void TreeGeneratorWidget::InstallGenerator(ITreeGeneratorPtr generator) {
  setWindowTitle(generator->Name(generator));
  d->model->InstallGenerator(std::move(generator));
}

void TreeGeneratorWidget::InstallModel(void) {
  d->model_proxy = new SearchFilterModelProxy(this);
  d->model_proxy->setRecursiveFilteringEnabled(true);
  d->model_proxy->setSourceModel(d->model);
  d->model_proxy->setDynamicSortFilter(true);

  connect(d->filter_settings_widget,
          &FilterSettingsWidget::ColumnFilterStateListChanged,
          d->model_proxy,
          &SearchFilterModelProxy::OnColumnFilterStateListChange);

  d->tree_view->setModel(d->model_proxy);

  connect(d->model_proxy, &QAbstractItemModel::modelReset,
          this, &TreeGeneratorWidget::OnModelReset);

  connect(d->model_proxy, &QAbstractItemModel::dataChanged,
          this, &TreeGeneratorWidget::OnDataChanged);

  connect(d->model_proxy, &QAbstractItemModel::rowsInserted,
          this, &TreeGeneratorWidget::OnRowsInserted);

  OnModelReset();
}

void TreeGeneratorWidget::InitializeWidgets(
    const ConfigManager &config_manager) {

  auto &theme_manager = config_manager.ThemeManager();
  auto &media_manager = config_manager.MediaManager();

  // Initialize the tree view
  d->tree_view = new QTreeView(this);
  d->tree_view->setSortingEnabled(true);
  d->tree_view->sortByColumn(0, Qt::AscendingOrder);

  // The auto scroll takes care of keeping the active item within the
  // visible viewport region. This is true for mouse clicks but also
  // keyboard navigation (i.e. arrow keys, page up/down, etc).
  d->tree_view->setAutoScroll(true);

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
  d->tree_view->installEventFilter(this);
  d->tree_view->viewport()->installEventFilter(this);
  d->tree_view->viewport()->setMouseTracking(true);

  // Make sure we can render tokens, if need be.
  config_manager.InstallItemDelegate(d->tree_view);

  // Initialize the treeview item buttons
  d->open = new QPushButton(QIcon(), "", this);
  d->open->setToolTip(tr("Open"));

  connect(d->open, &QPushButton::pressed, this,
          &TreeGeneratorWidget::OnOpenButtonPressed);

  d->expand = new QPushButton(QIcon(), "", this);
  d->expand->setToolTip(tr("Expand"));

  connect(d->expand, &QPushButton::pressed, this,
          &TreeGeneratorWidget::OnExpandButtonPressed);

  d->goto_ = new QPushButton(QIcon(), "", this);
  d->goto_->setToolTip(tr("Goto Original"));

  connect(d->goto_, &QPushButton::pressed,
          this, &TreeGeneratorWidget::OnGotoOriginalButtonPressed);

  // Create the search widget
  d->search_widget = new SearchWidget(
      media_manager, SearchWidget::Mode::Filter, this);

  connect(d->search_widget, &SearchWidget::SearchParametersChanged,
          this, &TreeGeneratorWidget::OnSearchParametersChange);

  // Create the search widget addon
  d->filter_settings_widget = new FilterSettingsWidget(d->model, this);

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
          d->model, &TreeGeneratorModel::CancelRunningRequest);

  connect(d->model, &TreeGeneratorModel::RequestStarted,
          this, &TreeGeneratorWidget::OnModelRequestStarted);

  connect(d->model, &TreeGeneratorModel::RequestFinished,
          this, &TreeGeneratorWidget::OnModelRequestFinished);

  d->status_widget->setLayout(status_widget_layout);

  // Setup the main layout
  setContentsMargins(0, 0, 0, 0);

  auto layout = new QVBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(d->tree_view, 1);
  layout->addStretch();
  layout->addWidget(d->status_widget);
  layout->addWidget(d->filter_settings_widget);
  layout->addWidget(d->search_widget);
  setLayout(layout);

  // Set the theme.
  connect(&theme_manager, &ThemeManager::ThemeChanged,
          this, &TreeGeneratorWidget::OnThemeChanged);

  OnThemeChanged(theme_manager);

  // Set the icons.
  connect(&media_manager, &MediaManager::IconsChanged,
          this, &TreeGeneratorWidget::OnIconsChanged);

  OnIconsChanged(media_manager);

  config_manager.InstallItemDelegate(d->tree_view);
}

//! Called when we want to act on the context menu.
void TreeGeneratorWidget::ActOnContextMenu(
    IWindowManager *, QMenu *menu, const QModelIndex &index) {

  auto selected_index = std::move(d->selected_index);
  if (index != selected_index) {
    return;
  }

  auto tooltip_var = index.data(Qt::ToolTipRole);
  if (tooltip_var.isValid()) {
    auto details = tooltip_var.toString();
    auto copy_details_action = new QAction(tr("Copy Details"), menu);
    menu->addAction(copy_details_action);
    connect(copy_details_action, &QAction::triggered,
            [=] (void) {
              qApp->clipboard()->setText(details);
            });
  }

  auto open_action = new QAction(tr("Open"), menu);
  menu->addAction(open_action);
  connect(open_action, &QAction::triggered,
          this, [=, this] (void) {
                  emit OpenItem(selected_index);
                });

  auto is_duplicate = index.data(TreeGeneratorModel::IsDuplicate);
  if (!is_duplicate.isValid() || !is_duplicate.toBool()) {

    auto i = 0u;
    auto can_expand = index.data(TreeGeneratorModel::CanBeExpanded);
    if (can_expand.isValid() && !can_expand.toBool()) {
      i = 1u;
    }

    QMenu *expand_menu = new QMenu(tr("Expand..."), menu);
    menu->addMenu(expand_menu);

    for (; i < kMaxExpansionLevel; ++i) {
      auto action = new QAction(tr("Expand &%1 levels").arg(i + 1u),
                                expand_menu);
      expand_menu->addAction(action);

      // TODO(alessandro): There's a Qt 6.x bug that prevents the &<N> from
      //                   working correctly, so for now we have to set the
      //                   shortcut explicitly
      action->setShortcut(static_cast<Qt::Key>(Qt::Key_1 + i));
      action->setToolTip(tr("Expands this entity for three levels"));
      action->setIcon(d->expand_item_icon_n[i]);

      connect(action, &QAction::triggered,
              this, [=, this] (void) {
                      d->model->Expand(selected_index, i + 1u);
                    });
    }
  } else {
    auto goto_action = new QAction(tr("Goto Original"), menu);
    menu->addAction(goto_action);
    connect(goto_action, &QAction::triggered,
            this, [=, this] (void) {
                    GotoOriginal(selected_index);
                  });
  }
}

bool TreeGeneratorWidget::eventFilter(QObject *obj, QEvent *event) {
  if (obj == d->tree_view) {
    // Disable the overlay buttons while scrolling. It is hard to keep
    // them on screen due to how the scrolling event is propagated.
    if (event->type() == QEvent::Wheel) {
      auto scrolling_enabled =
          (d->tree_view->horizontalScrollBar()->isVisible() ||
           d->tree_view->verticalScrollBar()->isVisible());

      if (scrolling_enabled) {
        UpdateItemButtons();
      }

      return false;

    } else if (event->type() != QEvent::KeyRelease) {
      return false;

    } else if (QKeyEvent *kevent = dynamic_cast<QKeyEvent *>(event)) {
      auto ret = false;
      for (auto index : d->tree_view->selectionModel()->selectedIndexes()) {
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
            d->model->Expand(d->model_proxy->mapToSource(index),
                             static_cast<unsigned>(kevent->key() - Qt::Key_0));
            ret = true;
            break;
          default: break;
        }
      }

      return ret;
    }

    return false;

  } else if (obj == d->tree_view->viewport()) {
    if (event->type() == QEvent::Leave || event->type() == QEvent::MouseMove) {
      UpdateItemButtons();
      return false;

    } else if (event->type() == QEvent::MouseButtonPress) {
      return true;

    } else if (event->type() == QEvent::MouseButtonRelease) {
      const auto &mouse_event = *static_cast<QMouseEvent *>(event);
      auto local_mouse_pos = mouse_event.position().toPoint();

      auto index = d->tree_view->indexAt(local_mouse_pos);
      if (!index.isValid()) {
        return true;
      }

      auto &selection_model = *d->tree_view->selectionModel();
      selection_model.setCurrentIndex(index,
          QItemSelectionModel::Clear | QItemSelectionModel::SelectCurrent);

      switch (mouse_event.button()) {
      case Qt::LeftButton: {
        OnItemClicked(index);
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
  if (d->updating_buttons) {
    return;
  }

  d->updating_buttons = true;
  d->open->setVisible(false);
  d->goto_->setVisible(false);
  d->expand->setVisible(false);

  // Disable the buttons while the model is updating.
  if (!d->model_proxy->dynamicSortFilter()) {
    d->updating_buttons = false;
    return;
  }

  // It is important to double check the leave event; it is sent
  // even if the mouse is still inside our treeview item but
  // above the hovering button (which steals the focus)
  auto mouse_pos = d->tree_view->viewport()->mapFromGlobal(QCursor::pos());
  auto index = d->tree_view->indexAt(mouse_pos);
  if (!d->model_proxy->mapToSource(index).isValid()) {
    d->updating_buttons = false;
    return;
  }

  // Enable the expansion button if we haven't yet expanded the node.
  // TODO(alessandro): Fix the button visibility
  auto expand_var = index.data(TreeGeneratorModel::CanBeExpanded);
  d->expand->setEnabled(expand_var.isValid() && expand_var.toBool());

  // Show/hide one of expand/goto if this is redundant.
  auto redundant_var = index.data(TreeGeneratorModel::IsDuplicate);
  auto is_redundant = redundant_var.isValid() && redundant_var.toBool();

  d->open->setVisible(true);
  d->goto_->setVisible(is_redundant);
  d->expand->setVisible(!is_redundant);

  // Keep up to date with TreeviewItemButtons
  static constexpr auto kNumButtons = 2u;
  QPushButton *button_list[kNumButtons] = {
      d->open,
      (is_redundant ? d->goto_
                    : d->expand),
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

  d->updating_buttons = false;
}

void TreeGeneratorWidget::OnIconsChanged(const MediaManager &media_manager) {
  d->open_item_icon = {};
  d->open_item_icon.addPixmap(
      media_manager.Pixmap("com.trailofbits.icon.Activate"),
      QIcon::Normal, QIcon::On);
  d->open_item_icon.addPixmap(
      media_manager.Pixmap("com.trailofbits.icon.Activate",
                           ITheme::IconStyle::DISABLED),
      QIcon::Disabled, QIcon::On);

  d->expand_item_icon = {};
  d->expand_item_icon.addPixmap(
      media_manager.Pixmap("com.trailofbits.icon.Expand"),
      QIcon::Normal, QIcon::On);
  d->expand_item_icon.addPixmap(
      media_manager.Pixmap("com.trailofbits.icon.Expand",
                           ITheme::IconStyle::DISABLED),
      QIcon::Disabled, QIcon::On);

  d->goto_item_icon = {};
  d->goto_item_icon.addPixmap(
      media_manager.Pixmap("com.trailofbits.icon.Goto"),
      QIcon::Normal, QIcon::On);
  d->goto_item_icon.addPixmap(
      media_manager.Pixmap("com.trailofbits.icon.Goto",
                           ITheme::IconStyle::DISABLED),
      QIcon::Disabled, QIcon::On);

  d->open->setIcon(d->open_item_icon);
  d->expand->setIcon(d->expand_item_icon);
  d->goto_->setIcon(d->goto_item_icon);

  for (auto i = 0u; i < kMaxExpansionLevel; ++i) {
    d->expand_item_icon_n[i] = {};

    auto icon_id = QString("com.trailofbits.icon.ExpandDepth%1").arg(i + 1u);

    d->expand_item_icon_n[i].addPixmap(
        media_manager.Pixmap(icon_id), QIcon::Normal, QIcon::On);

    d->expand_item_icon_n[i].addPixmap(
        media_manager.Pixmap(icon_id, ITheme::IconStyle::DISABLED),
        QIcon::Disabled, QIcon::On);
  }
}

void TreeGeneratorWidget::OnModelReset(void) {
  ExpandAllNodes();
  UpdateItemButtons();
}

void TreeGeneratorWidget::OnDataChanged(void) {
  UpdateItemButtons();
  ExpandAllNodes();
  d->tree_view->viewport()->repaint();
}

void TreeGeneratorWidget::ExpandAllNodes(void) {
  d->tree_view->expandAll();
  d->tree_view->resizeColumnToContents(0);
}

void TreeGeneratorWidget::OnRowsInserted(const QModelIndex &parent, int, int) {
  d->tree_view->expandRecursively(parent);
  d->tree_view->resizeColumnToContents(0);
}

void TreeGeneratorWidget::OnItemClicked(const QModelIndex &current_index) {
  auto new_index = d->model_proxy->mapToSource(current_index);
  if (!new_index.isValid()) {
    return;
  }
  
  // Suppress likely duplicate events.
  if (d->selection_timer.restart() < 100 &&
      d->selected_index == new_index) {
    return;
  }

  d->selected_index = new_index;
  emit RequestPrimaryClick(d->selected_index);
}

void TreeGeneratorWidget::OnOpenItemContextMenu(const QPoint &tree_local_mouse_pos) {
  auto index = d->tree_view->indexAt(tree_local_mouse_pos);
  d->selected_index = d->model_proxy->mapToSource(index);
  if (!d->selected_index.isValid()) {
    return;
  }

  emit RequestSecondaryClick(d->selected_index);
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

void TreeGeneratorWidget::OnOpenButtonPressed(void) {
  auto mouse_pos = d->tree_view->viewport()->mapFromGlobal(QCursor::pos());
  auto index = d->model_proxy->mapToSource(d->tree_view->indexAt(mouse_pos));
  if (!index.isValid()) {
    return;
  }

  emit OpenItem(index);
}

void TreeGeneratorWidget::OnExpandButtonPressed(void) {
  auto mouse_pos = d->tree_view->viewport()->mapFromGlobal(QCursor::pos());
  auto index = d->model_proxy->mapToSource(d->tree_view->indexAt(mouse_pos));
  if (!index.isValid()) {
    return;
  }

  d->model->Expand(index, 1u);
}

void TreeGeneratorWidget::GotoOriginal(const QModelIndex &index) {
  if (!index.isValid()) {
    return;
  }

  auto dedup = d->model->Deduplicate(index);
  Q_ASSERT(dedup != index);

  dedup = d->model_proxy->mapFromSource(dedup);
  if (!dedup.isValid()) {
    // TODO(pag): This seems to happen and I don't know why.
    return;
  }

  auto sel = d->tree_view->selectionModel();
  sel->clearSelection();
  sel->setCurrentIndex(dedup, QItemSelectionModel::Select);
  d->tree_view->scrollTo(dedup);
}

void TreeGeneratorWidget::OnGotoOriginalButtonPressed(void) {
  auto mouse_pos = d->tree_view->viewport()->mapFromGlobal(QCursor::pos());
  auto index = d->model_proxy->mapToSource(d->tree_view->indexAt(mouse_pos));
  GotoOriginal(index);
}

// NOTE(pag): The config manager handles the item delegate automatically.
void TreeGeneratorWidget::OnThemeChanged(const ThemeManager &theme_manager) {
  setFont(theme_manager.Theme()->Font());
}

void TreeGeneratorWidget::OnModelRequestStarted(void) {
  d->status_widget->setVisible(true);
  d->model_proxy->setDynamicSortFilter(false);
}

void TreeGeneratorWidget::OnModelRequestFinished(void) {
  d->status_widget->setVisible(false);
  d->model_proxy->setDynamicSortFilter(true);
}

//! Used to hide the OSD buttons when focus is lost
void TreeGeneratorWidget::focusOutEvent(QFocusEvent *) {
  UpdateItemButtons();
}

}  // namespace mx::gui