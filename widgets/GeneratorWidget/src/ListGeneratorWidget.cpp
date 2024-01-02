/*
  Copyright (c) 2024-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/GUI/Widgets/ListGeneratorWidget.h>

#include <QAction>
#include <QClipboard>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QPushButton>
#include <QScrollBar>

#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/MediaManager.h>
#include <multiplier/GUI/Widgets/FilterSettingsWidget.h>
#include <multiplier/GUI/Widgets/SearchWidget.h>

#include "SearchFilterModelProxy.h"
#include "ListGeneratorModel.h"

namespace mx::gui {

struct ListGeneratorWidget::PrivateData final {
  ListGeneratorModel *model{nullptr};
  SearchFilterModelProxy *model_proxy{nullptr};

  QListView *list_widget{nullptr};
  SearchWidget *search_widget{nullptr};
  FilterSettingsWidget *filter_settings_widget{nullptr};
  QWidget *status_widget{nullptr};

  bool updating_buttons{false};

  // Keep up to date with UpdateTreeViewItemButtons
  QPushButton *goto_{nullptr};
  QIcon goto_item_icon;

  QModelIndex hovered_index;
  QModelIndex selected_index;
};

ListGeneratorWidget::~ListGeneratorWidget(void) {}

ListGeneratorWidget::ListGeneratorWidget(
    const ConfigManager &config_manager, QWidget *parent)
    : QWidget(parent),
      d(new PrivateData) {

  d->model = new ListGeneratorModel(this);
  InitializeWidgets(config_manager);
  InstallModel();

  // Synchronize the search widget and its addon
  d->search_widget->Deactivate();
}

//! Install a new generator.
void ListGeneratorWidget::InstallGenerator(IListGeneratorPtr generator) {
  d->model->InstallGenerator(std::move(generator));
}

void ListGeneratorWidget::InstallModel(void) {
  connect(d->model, &ListGeneratorModel::NameChanged,
          [=] (QString new_name) {
            setWindowTitle(new_name);
          });

  d->model_proxy = new SearchFilterModelProxy(this);
  d->model_proxy->setRecursiveFilteringEnabled(true);
  d->model_proxy->setSourceModel(d->model);
  d->model_proxy->setDynamicSortFilter(true);

  connect(d->filter_settings_widget,
          &FilterSettingsWidget::ColumnFilterStateListChanged,
          d->model_proxy,
          &SearchFilterModelProxy::OnColumnFilterStateListChange);

  d->list_widget->setModel(d->model_proxy);

  // Note: this needs to happen after the model has been set in the
  // tree view!
  auto tree_selection_model = d->list_widget->selectionModel();
  connect(tree_selection_model, &QItemSelectionModel::currentChanged,
          this, &ListGeneratorWidget::OnCurrentItemChanged);

  connect(d->model_proxy, &QAbstractItemModel::modelReset,
          this, &ListGeneratorWidget::OnModelReset);

  connect(d->model_proxy, &QAbstractItemModel::dataChanged,
          this, &ListGeneratorWidget::OnDataChanged);

  OnModelReset();
}

void ListGeneratorWidget::InitializeWidgets(
    const ConfigManager &config_manager) {

  auto &theme_manager = config_manager.ThemeManager();
  auto &media_manager = config_manager.MediaManager();

  // Initialize the list view
  d->list_widget = new QListView(this);

  // The auto scroll takes care of keeping the active item within the
  // visible viewport region. This is true for mouse clicks but also
  // keyboard navigation (i.e. arrow keys, page up/down, etc).
  d->list_widget->setAutoScroll(false);

  // Smooth scrolling.
  d->list_widget->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  d->list_widget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

  // Disallow multiple selection. If we have grouping by file enabled, then when
  // a user clicks on a file name, we instead jump down to the first entry
  // grouped under that file. This is to make using the up/down arrows easier.
  d->list_widget->setSelectionBehavior(QAbstractItemView::SelectRows);
  d->list_widget->setSelectionMode(QAbstractItemView::SingleSelection);
  d->list_widget->setTextElideMode(Qt::TextElideMode::ElideRight);

  d->list_widget->setAlternatingRowColors(false);
  d->list_widget->installEventFilter(this);
  d->list_widget->viewport()->installEventFilter(this);
  d->list_widget->viewport()->setMouseTracking(true);

  d->list_widget->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(d->list_widget, &QListView::customContextMenuRequested,
          this, &ListGeneratorWidget::OnOpenItemContextMenu);

  connect(d->list_widget, &QAbstractItemView::clicked,
          [this] (const QModelIndex &index) {
            OnCurrentItemChanged(index, {});
          });

  // Make sure we can render tokens, if need be.
  config_manager.InstallItemDelegate(d->list_widget);

  d->goto_ = new QPushButton(QIcon(), "", this);
  d->goto_->setToolTip(tr("Goto Original"));

  connect(d->goto_, &QPushButton::pressed,
          this, &ListGeneratorWidget::OnGotoOriginalButtonPressed);

  // Create the search widget
  d->search_widget = new SearchWidget(
      media_manager, SearchWidget::Mode::Filter, this);

  connect(d->search_widget, &SearchWidget::SearchParametersChanged,
          this, &ListGeneratorWidget::OnSearchParametersChange);

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
          d->model, &ListGeneratorModel::CancelRunningRequest);

  connect(d->model, &ListGeneratorModel::RequestStarted,
          this, &ListGeneratorWidget::OnModelRequestStarted);

  connect(d->model, &ListGeneratorModel::RequestFinished,
          this, &ListGeneratorWidget::OnModelRequestFinished);

  d->status_widget->setLayout(status_widget_layout);

  // Setup the main layout
  setContentsMargins(0, 0, 0, 0);

  auto layout = new QVBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(d->list_widget, 1);
  layout->addStretch();
  layout->addWidget(d->status_widget);
  layout->addWidget(d->filter_settings_widget);
  layout->addWidget(d->search_widget);
  setLayout(layout);

  // Set the theme.
  connect(&theme_manager, &ThemeManager::ThemeChanged,
          this, &ListGeneratorWidget::OnThemeChanged);

  OnThemeChanged(theme_manager);

  // Set the icons.
  connect(&media_manager, &MediaManager::IconsChanged,
          this, &ListGeneratorWidget::OnIconsChanged);

  OnIconsChanged(media_manager);

  config_manager.InstallItemDelegate(d->list_widget);
}

//! Called when we want to act on the context menu.
void ListGeneratorWidget::ActOnContextMenu(
    QMenu *menu, const QModelIndex &index) {
  if (index != d->selected_index) {
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

  auto sort_menu = new QMenu(tr("Sort..."), menu);
  auto sort_ascending_order = new QAction(tr("Ascending Order"), sort_menu);
  sort_menu->addAction(sort_ascending_order);

  auto sort_descending_order = new QAction(tr("Descending Order"), sort_menu);
  sort_menu->addAction(sort_descending_order);

  menu->addMenu(sort_menu);

  connect(sort_ascending_order, &QAction::triggered,
          [this] (void) {
            d->model_proxy->sort(0, Qt::AscendingOrder);
          });

  connect(sort_descending_order, &QAction::triggered,
          [this] (void) {
            d->model_proxy->sort(0, Qt::DescendingOrder);
          });
}

bool ListGeneratorWidget::eventFilter(QObject *obj, QEvent *event) {
  if (obj == d->list_widget) {
    // Disable the overlay buttons while scrolling. It is hard to keep
    // them on screen due to how the scrolling event is propagated.
    if (event->type() == QEvent::Wheel) {
      auto scrolling_enabled =
          (d->list_widget->horizontalScrollBar()->isVisible() ||
           d->list_widget->verticalScrollBar()->isVisible());

      if (scrolling_enabled) {
        d->hovered_index = {};
        UpdateItemButtons();
      }

      return false;

    } else if (event->type() != QEvent::KeyRelease) {
      return false;
    }

    return false;

  } else if (obj == d->list_widget->viewport()) {
    if (event->type() == QEvent::Leave || event->type() == QEvent::MouseMove) {
      // It is important to double check the leave event; it is sent
      // even if the mouse is still inside our treeview item but
      // above the hovering button (which steals the focus)
      auto mouse_pos = d->list_widget->viewport()->mapFromGlobal(QCursor::pos());

      auto index = d->list_widget->indexAt(mouse_pos);
      if (!index.isValid()) {
        d->hovered_index = {};
      } else {
        d->hovered_index = d->model_proxy->mapToSource(index);
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

void ListGeneratorWidget::resizeEvent(QResizeEvent *) {
  UpdateItemButtons();
}

void ListGeneratorWidget::UpdateItemButtons(void) {

  // TODO(pag): Sometimes we get infinite recursion from Qt doing a
  // `sendSyntheticEnterLeave` below when we `setVisible(is_redundant)`.
  if (d->updating_buttons) {
    return;
  }

  d->updating_buttons = true;
  d->goto_->setVisible(false);

  // Always show the buttons, but disable the ones that are not
  // applicable. This is to prevent buttons from disappearing and/or
  // reordering while the user is clicking them
  auto display_buttons = d->hovered_index.isValid();
  if (!display_buttons) {
    d->updating_buttons = false;
    return;
  }

  // Show/hide one of goto if this is redundant.
  const auto &index = d->hovered_index;
  auto redundant_var = index.data(ListGeneratorModel::IsDuplicate);
  if (!redundant_var.isValid() || !redundant_var.toBool()) {
    d->updating_buttons = false;
    return;
  }

  d->goto_->setVisible(true);

  // Update the button positions
  auto rect = d->list_widget->visualRect(index);

  auto button_margin = rect.height() / 6;
  auto button_size = rect.height() - (button_margin * 2);
  auto button_area_width = button_size + button_margin;

  auto current_x =
      d->list_widget->pos().x() + d->list_widget->width() - button_area_width;

  const auto &vertical_scrollbar = *d->list_widget->verticalScrollBar();
  if (vertical_scrollbar.isVisible()) {
    current_x -= vertical_scrollbar.width();
  }

  auto current_y = rect.y() + (rect.height() / 2) - (button_size / 2);

  auto pos = mapFromGlobal(
      d->list_widget->viewport()->mapToGlobal(QPoint(current_x, current_y)));

  d->goto_->resize(button_size, button_size);
  d->goto_->move(pos.x(), pos.y());
  d->goto_->raise();
  d->updating_buttons = false;
}

void ListGeneratorWidget::OnIconsChanged(const MediaManager &media_manager) {
  d->goto_item_icon = {};
  d->goto_item_icon.addPixmap(
      media_manager.Pixmap("com.trailofbits.icon.Goto"),
      QIcon::Normal, QIcon::On);
  d->goto_item_icon.addPixmap(
      media_manager.Pixmap("com.trailofbits.icon.Goto",
                           ITheme::IconStyle::DISABLED),
      QIcon::Disabled, QIcon::On);

  d->goto_->setIcon(d->goto_item_icon);
}

void ListGeneratorWidget::OnModelReset(void) {
  d->hovered_index = {};
  UpdateItemButtons();
}

void ListGeneratorWidget::OnDataChanged(void) {
  UpdateItemButtons();

  d->list_widget->viewport()->repaint();
}

void ListGeneratorWidget::OnCurrentItemChanged(const QModelIndex &current_index,
                                               const QModelIndex &) {
  d->selected_index = d->model_proxy->mapToSource(current_index);
  if (!d->selected_index.isValid()) {
    return;
  }

  emit SelectedItemChanged(d->selected_index);
}

void ListGeneratorWidget::OnOpenItemContextMenu(const QPoint &point) {
  auto index = d->list_widget->indexAt(point);
  d->selected_index = d->model_proxy->mapToSource(index);
  if (!d->selected_index.isValid()) {
    return;
  }

  emit RequestContextMenu(d->selected_index);
}

void ListGeneratorWidget::OnSearchParametersChange(void) {
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
}

void ListGeneratorWidget::OnGotoOriginalButtonPressed(void) {
  if (!d->hovered_index.isValid()) {
    return;
  }

  auto dedup = d->model->Deduplicate(d->hovered_index);
  dedup = d->model_proxy->mapFromSource(dedup);
  if (!dedup.isValid()) {
    return;
  }

  auto sel = d->list_widget->selectionModel();
  sel->clearSelection();
  sel->setCurrentIndex(dedup, QItemSelectionModel::Select);
  d->list_widget->scrollTo(dedup);
}

// NOTE(pag): The config manager handles the item delegate automatically.
void ListGeneratorWidget::OnThemeChanged(const ThemeManager &theme_manager) {
  setFont(theme_manager.Theme()->Font());
  d->model->OnThemeChanged(theme_manager);
}

void ListGeneratorWidget::OnModelRequestStarted(void) {
  d->status_widget->setVisible(true);
  d->model_proxy->setDynamicSortFilter(false);
}

void ListGeneratorWidget::OnModelRequestFinished(void) {
  d->status_widget->setVisible(false);
  d->model_proxy->setDynamicSortFilter(true);
}

//! Used to hide the OSD buttons when focus is lost
void ListGeneratorWidget::focusOutEvent(QFocusEvent *) {
  d->hovered_index = {};
  UpdateItemButtons();
}

}  // namespace mx::gui