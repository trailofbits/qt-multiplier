// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "MacroExplorer.h"
#include "ExpandedMacrosModel.h"

#include <multiplier/GUI/Managers/ActionManager.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/MediaManager.h>
#include <multiplier/GUI/Util.h>

#include <multiplier/Index.h>

#include <QEvent>
#include <QHeaderView>
#include <QPoint>
#include <QPushButton>
#include <QScrollBar>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QVBoxLayout>
#include <QMouseEvent>

namespace mx::gui {

struct MacroExplorer::PrivateData {
  QTableView *table{nullptr};
  QIcon close_item_icon;
  QIcon open_item_icon;
  QPushButton *open{nullptr};
  QPushButton *close{nullptr};

  ExpandedMacrosModel *model{nullptr};
  QSortFilterProxyModel *model_proxy{nullptr};

  TriggerHandle open_entity_trigger;

  bool updating_buttons{false};
};

MacroExplorer::~MacroExplorer(void) {}

MacroExplorer::MacroExplorer(const ConfigManager &config_manager,
                             ExpandedMacrosModel *model,
                             QWidget *parent)
    : IWindowWidget(parent),
      d(new PrivateData) {

  d->open_entity_trigger = config_manager.ActionManager().Find(
        "com.trailofbits.action.OpenEntity");

  d->table = new QTableView(this);

  d->table->setAlternatingRowColors(false);
  d->table->setSelectionBehavior(QAbstractItemView::SelectRows);
  d->table->setSelectionMode(QAbstractItemView::SingleSelection);
  d->table->setTextElideMode(Qt::TextElideMode::ElideRight);

  // The auto scroll takes care of keeping the active item within the
  // visible viewport region. This is true for mouse clicks but also
  // keyboard navigation (i.e. arrow keys, page up/down, etc).
  d->table->setAutoScroll(true);

  // Smooth scrolling.
  d->table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  d->table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

  // Disallow multiple selection. If we have grouping by file enabled, then when
  // a user clicks on a file name, we instead jump down to the first entry
  // grouped under that file. This is to make using the up/down arrows easier.
  d->table->setSelectionBehavior(QAbstractItemView::SelectRows);
  d->table->setSelectionMode(QAbstractItemView::SingleSelection);

  // Stretch the last section.
  d->table->horizontalHeader()->setStretchLastSection(true);

  // Set up the model, enabling sorting.
  d->model = model;
  d->model_proxy = new QSortFilterProxyModel(d->table);
  d->model_proxy->setSourceModel(d->model);
  d->table->setModel(d->model_proxy);
  d->table->setSortingEnabled(true);

  config_manager.InstallItemDelegate(d->table);

  auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(d->table, 1);
  layout->addStretch();

  // Initialize the item buttons
  d->open = new QPushButton(QIcon(), "", this);
  d->open->setToolTip(tr("Open"));

  d->close = new QPushButton(QIcon(), "", this);
  d->close->setToolTip(tr("Remove / Unexpand"));

  connect(d->open, &QPushButton::pressed, this,
          &MacroExplorer::OnOpenButtonPressed);

  connect(d->close, &QPushButton::pressed, this,
          &MacroExplorer::OnCloseButtonPressed);

  auto &media_manager = config_manager.MediaManager();
  OnIconsChanged(media_manager);
  UpdateItemButtons();

  // Manages dynamically showing/hiding of the buttons.
  d->table->installEventFilter(this);
  d->table->viewport()->installEventFilter(this);
  d->table->viewport()->setMouseTracking(true);

  setContentsMargins(0, 0, 0, 0);
  setLayout(layout);
  setWindowTitle(tr("Macro Explorer"));
}

void MacroExplorer::OnIconsChanged(const MediaManager &media_manager) {
  d->open_item_icon = {};
  d->open_item_icon.addPixmap(
      media_manager.Pixmap("com.trailofbits.icon.Activate"),
      QIcon::Normal, QIcon::On);
  d->open_item_icon.addPixmap(
      media_manager.Pixmap("com.trailofbits.icon.Activate",
                           ITheme::IconStyle::DISABLED),
      QIcon::Disabled, QIcon::On);

  d->close_item_icon = {};
  d->close_item_icon.addPixmap(
      media_manager.Pixmap("com.trailofbits.icon.Close"),
      QIcon::Normal, QIcon::On);
  d->close_item_icon.addPixmap(
      media_manager.Pixmap("com.trailofbits.icon.Close",
                           ITheme::IconStyle::DISABLED),
      QIcon::Disabled, QIcon::On);

  d->open->setIcon(d->open_item_icon);
  d->close->setIcon(d->close_item_icon);
}

// Trigger the macro to be opened.
void MacroExplorer::OnOpenButtonPressed(void) {
  auto mouse_pos = d->table->viewport()->mapFromGlobal(QCursor::pos());
  auto index = d->model_proxy->mapToSource(d->table->indexAt(mouse_pos));
  if (!index.isValid()) {
    return;
  }

  auto entity = IModel::Entity(index);
  if (!std::holds_alternative<Macro>(entity)) {
    return;
  }

  d->open_entity_trigger.Trigger(QVariant::fromValue(entity));
}

// Remove the macro from the expansion list.
void MacroExplorer::OnCloseButtonPressed(void) {
  auto mouse_pos = d->table->viewport()->mapFromGlobal(QCursor::pos());
  auto index = d->model_proxy->mapToSource(d->table->indexAt(mouse_pos));
  if (!index.isValid()) {
    return;
  }

  auto entity = IModel::Entity(index);
  if (!std::holds_alternative<Macro>(entity)) {
    return;
  }

  d->open->setVisible(false);
  d->close->setVisible(false);
  d->model->RemoveMacro(std::move(std::get<Macro>(entity)));
}

bool MacroExplorer::eventFilter(QObject *obj, QEvent *event) {
  if (obj == d->table) {
    // Disable the overlay buttons while scrolling. It is hard to keep
    // them on screen due to how the scrolling event is propagated.
    if (event->type() == QEvent::Wheel) {
      if (d->table->horizontalScrollBar()->isVisible() ||
          d->table->verticalScrollBar()->isVisible()) {
        UpdateItemButtons();
      }
    }

  } else if (obj == d->table->viewport()) {
    if (event->type() == QEvent::Leave || event->type() == QEvent::MouseMove) {
      UpdateItemButtons();

    } else if (event->type() == QEvent::MouseButtonPress) {
      const auto mouse_event = static_cast<QMouseEvent *>(event);
      if ((mouse_event->buttons() & Qt::RightButton) != 0) {
        auto local_mouse_pos{mouse_event->pos()};

        auto model_index = d->table->indexAt(local_mouse_pos);
        if (!model_index.isValid()){
          return false;
        }

        auto global_mouse_pos{d->table->viewport()->mapToGlobal(local_mouse_pos)};
        OnContextMenu(global_mouse_pos, model_index);
      }
    }
  }
  return false;
}

void MacroExplorer::resizeEvent(QResizeEvent *) {
  UpdateItemButtons();
}

void MacroExplorer::focusOutEvent(QFocusEvent *) {
  UpdateItemButtons();
}

void MacroExplorer::UpdateItemButtons(void) {

  // TODO(pag): Sometimes we get infinite recursion from Qt doing a
  // `sendSyntheticEnterLeave` below when we `setVisible(is_redundant)`.
  if (d->updating_buttons) {
    return;
  }

  d->updating_buttons = true;
  d->open->setVisible(false);
  d->close->setVisible(false);

  // Disable the buttons while the model is updating.
  if (!d->model_proxy->dynamicSortFilter()) {
    d->updating_buttons = false;
    return;
  }

  // It is important to double check the leave event; it is sent
  // even if the mouse is still inside our table item but
  // above the hovering button (which steals the focus)
  auto mouse_pos = d->table->viewport()->mapFromGlobal(QCursor::pos());
  auto index = d->table->indexAt(mouse_pos);
  if (!d->model_proxy->mapToSource(index).isValid()) {
    d->updating_buttons = false;
    return;
  }

  d->open->setVisible(true);
  d->close->setVisible(true);

  // Keep up to date with TreeviewItemButtons
  static constexpr auto kNumButtons = 2u;
  QPushButton *button_list[kNumButtons] = {d->open, d->close};

  // Update the button positions
  auto rect = d->table->visualRect(index);
  auto button_margin = rect.height() / 6;
  auto button_size = rect.height() - (button_margin * 2);
  auto button_count = static_cast<int>(kNumButtons);
  auto button_area_width =
      (button_count * button_size) + (button_count * button_margin);

  auto current_x =
      d->table->pos().x() + d->table->width() - button_area_width;

  const auto &vertical_scrollbar = *d->table->verticalScrollBar();
  if (vertical_scrollbar.isVisible()) {
    current_x -= vertical_scrollbar.width();
  }

  auto current_y = rect.y() + (rect.height() / 2) - (button_size / 2);

  auto pos = d->table->viewport()->mapToGlobal(QPoint(current_x, current_y));

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

void
MacroExplorer::OnContextMenu(const QPoint &pos, const QModelIndex &index) {
  auto menu = new QMenu(tr("Context Menu"), this);
  GenerateCopySubMenu(menu, index);

  menu->exec(pos);
}

}  // namespace mx::gui
