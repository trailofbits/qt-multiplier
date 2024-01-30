/*
  Copyright (c) 2024-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/GUI/Widgets/ListGeneratorWidget.h>

#include <QAction>
#include <QClipboard>
#include <QElapsedTimer>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QPushButton>
#include <QScrollBar>

#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/MediaManager.h>
#include <multiplier/GUI/Widgets/SearchWidget.h>

#include "SearchFilterModelProxy.h"
#include "ListGeneratorModel.h"

namespace mx::gui {
namespace {

// Activate the selected index when pressing this key
static constexpr auto kActivateSelectedItem{Qt::Key_Return};

// Allow users to avoid activating an item with a click by holding this key down
static constexpr auto kDisableClickActivationModifier{Qt::ControlModifier};

}

struct ListGeneratorWidget::PrivateData final {
  ListGeneratorModel *model{nullptr};
  SearchFilterModelProxy *model_proxy{nullptr};

  QListView *list_view{nullptr};
  SearchWidget *search_widget{nullptr};
  QWidget *status_widget{nullptr};

  bool updating_buttons{false};

  // Keep up to date with UpdateTreeViewItemButtons
  QPushButton *goto_{nullptr};
  QIcon goto_item_icon;

  QModelIndex selected_index;
  QElapsedTimer selection_timer;
};

ListGeneratorWidget::~ListGeneratorWidget(void) {}

ListGeneratorWidget::ListGeneratorWidget(
    const ConfigManager &config_manager, const QString &model_id, QWidget *parent)
    : IWindowWidget(parent),
      d(new PrivateData) {

  d->selection_timer.start();
  d->model = new ListGeneratorModel(model_id, this);
  InitializeWidgets(config_manager);
  InstallModel();

  // Synchronize the search widget and its addon
  d->search_widget->Deactivate();
}

//! Install a new generator.
void ListGeneratorWidget::InstallGenerator(IListGeneratorPtr generator) {
  setWindowTitle(generator->Name(generator));
  d->model->InstallGenerator(std::move(generator));
}

void ListGeneratorWidget::InstallModel(void) {
  d->model_proxy = new SearchFilterModelProxy(this);
  d->model_proxy->setRecursiveFilteringEnabled(true);
  d->model_proxy->setSourceModel(d->model);
  d->model_proxy->setDynamicSortFilter(true);

  d->list_view->setModel(d->model_proxy);

  // Note: this needs to happen after the model has been set in the
  // tree view!
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
  d->list_view = new QListView(this);

  // Make it auto stretch.
  d->list_view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  // The auto scroll takes care of keeping the active item within the
  // visible viewport region. This is true for mouse clicks but also
  // keyboard navigation (i.e. arrow keys, page up/down, etc).
  d->list_view->setAutoScroll(false);

  // Smooth scrolling.
  d->list_view->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  d->list_view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

  // Disallow multiple selection. If we have grouping by file enabled, then when
  // a user clicks on a file name, we instead jump down to the first entry
  // grouped under that file. This is to make using the up/down arrows easier.
  d->list_view->setSelectionBehavior(QAbstractItemView::SelectRows);
  d->list_view->setSelectionMode(QAbstractItemView::SingleSelection);
  d->list_view->setTextElideMode(Qt::TextElideMode::ElideRight);

  d->list_view->setAlternatingRowColors(false);
  d->list_view->installEventFilter(this);
  d->list_view->viewport()->installEventFilter(this);
  d->list_view->viewport()->setMouseTracking(true);

  // Make sure we can render tokens, if need be.
  config_manager.InstallItemDelegate(d->list_view);

  d->goto_ = new QPushButton(QIcon(), "", this);
  d->goto_->setToolTip(tr("Goto Original"));

  connect(d->goto_, &QPushButton::pressed,
          this, &ListGeneratorWidget::OnGotoOriginalButtonPressed);

  // Create the search widget
  d->search_widget = new SearchWidget(
      media_manager, SearchWidget::Mode::Filter, this);

  connect(d->search_widget, &SearchWidget::SearchParametersChanged,
          this, &ListGeneratorWidget::OnSearchParametersChange);

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
  layout->addWidget(d->list_view, 1);
  layout->addWidget(d->status_widget);
  layout->addWidget(d->search_widget);
  setLayout(layout);

  // Set the theme.
  connect(&theme_manager, &ThemeManager::ThemeChanged,
          this, &ListGeneratorWidget::OnThemeChanged);

  OnThemeChanged(theme_manager);

  config_manager.InstallItemDelegate(d->list_view);

  // Set the icons.
  connect(&media_manager, &MediaManager::IconsChanged,
          this, &ListGeneratorWidget::OnIconsChanged);

  OnIconsChanged(media_manager);
}

//! Called when we want to act on the context menu.
void ListGeneratorWidget::ActOnContextMenu(
    IWindowManager *, QMenu *menu, const QModelIndex &index) {
  
  auto selected_index = std::move(d->selected_index);

  if (index.isValid() && index == selected_index) {
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
  }

  auto vp = d->list_view->viewport();
  if (vp->rect().contains(vp->mapFromGlobal(menu->pos()))) {
    auto sort_menu = new QMenu(tr("Sort..."), menu);
    auto sort_ascending_order = new QAction(tr("Ascending Order"), sort_menu);
    sort_menu->addAction(sort_ascending_order);

    auto sort_descending_order = new QAction(tr("Descending Order"), sort_menu);
    sort_menu->addAction(sort_descending_order);

    menu->addMenu(sort_menu);

    connect(sort_ascending_order, &QAction::triggered,
            this, [this] (void) {
                    d->model_proxy->sort(0, Qt::AscendingOrder);
                  });

    connect(sort_descending_order, &QAction::triggered,
            this, [this] (void) {
                    d->model_proxy->sort(0, Qt::DescendingOrder);
                  });
  }
}

bool ListGeneratorWidget::eventFilter(QObject *obj, QEvent *event) {
  if (obj == d->list_view) {
    // Disable the overlay buttons while scrolling. It is hard to keep
    // them on screen due to how the scrolling event is propagated.
    if (event->type() == QEvent::Wheel) {
      auto scrolling_enabled =
          (d->list_view->horizontalScrollBar()->isVisible() ||
           d->list_view->verticalScrollBar()->isVisible());

      if (scrolling_enabled) {
        UpdateItemButtons();
      }

      return false;

    } else if (event->type() == QEvent::KeyRelease) {
      const auto &selection_model = *d->list_view->selectionModel();
      auto index = selection_model.currentIndex();
      if (!index.isValid()) {
        return false;
      }

      const auto &key_event = *static_cast<QKeyEvent *>(event);
      switch (key_event.keyCombination().key()) {
      case kActivateSelectedItem:
        OnItemActivated(index);
        return true;

      default:
        return false;
      }

    } else {
      return false;
    }

  } else if (obj == d->list_view->viewport()) {
    if (event->type() == QEvent::Leave || event->type() == QEvent::MouseMove) {
      UpdateItemButtons();
      return false;

    } else if (event->type() == QEvent::MouseButtonPress) {
      return true;

    } else if (event->type() == QEvent::MouseButtonRelease) {
      const auto &mouse_event = *static_cast<QMouseEvent *>(event);
      auto local_mouse_pos = mouse_event.position().toPoint();

      auto index = d->list_view->indexAt(local_mouse_pos);
      if (!index.isValid()) {
        return true;
      }

      auto &selection_model = *d->list_view->selectionModel();
      selection_model.setCurrentIndex(index,
          QItemSelectionModel::Clear | QItemSelectionModel::SelectCurrent);

      switch (mouse_event.button()) {
        case Qt::LeftButton: {
          if ((mouse_event.modifiers() & kDisableClickActivationModifier) == 0) {
            OnItemActivated(index);
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
    }
  }

  return false;
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

  auto mouse_pos = d->list_view->viewport()->mapFromGlobal(QCursor::pos());
  auto index = d->list_view->indexAt(mouse_pos);
  if (!d->model_proxy->mapToSource(index).isValid()) {
    d->updating_buttons = false;
    return;
  }

  // Show/hide one of goto if this is redundant.
  auto redundant_var = index.data(ListGeneratorModel::IsDuplicate);
  if (!redundant_var.isValid() || !redundant_var.toBool()) {
    d->updating_buttons = false;
    return;
  }

  d->goto_->setVisible(true);

  // Update the button positions
  auto rect = d->list_view->visualRect(index);

  auto button_margin = rect.height() / 6;
  auto button_size = rect.height() - (button_margin * 2);
  auto button_area_width = button_size + button_margin;

  auto current_x =
      d->list_view->pos().x() + d->list_view->width() - button_area_width;

  const auto &vertical_scrollbar = *d->list_view->verticalScrollBar();
  if (vertical_scrollbar.isVisible()) {
    current_x -= vertical_scrollbar.width();
  }

  auto current_y = rect.y() + (rect.height() / 2) - (button_size / 2);

  auto pos = mapFromGlobal(
      d->list_view->viewport()->mapToGlobal(QPoint(current_x, current_y)));

  d->goto_->resize(button_size, button_size);
  d->goto_->move(pos.x(), pos.y());
  d->goto_->raise();
  d->updating_buttons = false;
}

void ListGeneratorWidget::OnIconsChanged(const MediaManager &media_manager) {
  // Note that we'll only get the height since we are passing in an invalid
  // model index
  auto cell_size_hint = d->list_view->itemDelegate()
                                    ->sizeHint(QStyleOptionViewItem{}, QModelIndex());

  auto pixmap = media_manager.Pixmap("com.trailofbits.icon.Goto");

  auto button_margin = cell_size_hint.height() / 6;
  auto required_width = cell_size_hint.height() - (button_margin * 2);

  auto icon_size = pixmap.deviceIndependentSize()
                         .scaled(required_width, required_width, Qt::KeepAspectRatio);

  d->goto_item_icon = {};
  d->goto_item_icon.addPixmap(std::move(pixmap), QIcon::Normal, QIcon::On);
  d->goto_item_icon.addPixmap(
      media_manager.Pixmap("com.trailofbits.icon.Goto",
                           ITheme::IconStyle::DISABLED),
      QIcon::Disabled, QIcon::On);

  d->goto_->setIcon(d->goto_item_icon);
  d->goto_->setIconSize(icon_size.toSize());
}

void ListGeneratorWidget::OnModelReset(void) {
  UpdateItemButtons();
}

void ListGeneratorWidget::OnDataChanged(void) {
  UpdateItemButtons();

  d->list_view->viewport()->repaint();
}

void ListGeneratorWidget::OnItemActivated(const QModelIndex &current_index) {
  // Suppress likely duplicate events.
  if (d->selection_timer.restart() < 100 &&
      d->selected_index == current_index) {
    return;
  }

  d->selected_index = current_index;
  emit RequestPrimaryClick(d->selected_index);
}

void ListGeneratorWidget::OnOpenItemContextMenu(
    const QPoint &list_local_mouse_pos) {
  d->selected_index = d->list_view->indexAt(list_local_mouse_pos);
  if (!d->selected_index.isValid()) {
    return;
  }

  emit RequestSecondaryClick(d->selected_index);
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
  auto mouse_pos = d->list_view->viewport()->mapFromGlobal(QCursor::pos());
  auto index = d->model_proxy->mapToSource(d->list_view->indexAt(mouse_pos));
  if (!index.isValid()) {
    d->updating_buttons = false;
    return;
  }

  auto dedup = d->model->Deduplicate(index);
  dedup = d->model_proxy->mapFromSource(dedup);
  if (!dedup.isValid()) {
    return;
  }

  auto sel = d->list_view->selectionModel();
  sel->clearSelection();
  sel->setCurrentIndex(dedup, QItemSelectionModel::Select);
  d->list_view->scrollTo(dedup);
  d->goto_->hide();
}

// NOTE(pag): The config manager handles the item delegate automatically.
void ListGeneratorWidget::OnThemeChanged(const ThemeManager &theme_manager) {
  setFont(theme_manager.Theme()->Font());
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
  UpdateItemButtons();
}

}  // namespace mx::gui