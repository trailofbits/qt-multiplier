/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/GUI/Widgets/HistoryWidget.h>

#include <multiplier/AST/Decl.h>
#include <multiplier/Frontend/Compilation.h>
#include <multiplier/Frontend/File.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/MediaManager.h>

#include <QHBoxLayout>
#include <QIcon>
#include <QMenu>
#include <QThreadPool>
#include <QToolButton>
#include <QShortcut>

#include <optional>
#include <atomic>
#include <cstdint>
#include <list>
#include <vector>

#include "HistoryLabelBuilder.h"

namespace mx::gui {
namespace {

// Global next item IDs. There will never be repeats.
static std::atomic<uint64_t> gNextItemId(0);

static const char *const kBackButtonToolTip =
    "Go back in the navigation history";

static const char *const kForwardButtonToolTip =
    "Go forward in the navigation history";

struct Item final {
  uint64_t item_id;
  VariantEntity original_entity;
  VariantEntity canonical_entity;
  QString name;

  inline Item(VariantEntity original_entity_, VariantEntity canonical_entity_,
              const QString &name_)
      : item_id(gNextItemId.fetch_add(1u)),
        original_entity(std::move(original_entity_)),
        canonical_entity(std::move(canonical_entity_)),
        name(name_) {}
};

using ItemList = std::list<Item>;

}  // namespace

struct HistoryWidget::PrivateData {
  FileLocationCache file_cache;

  const unsigned max_history_size;
  ItemList item_list;

  // NOTE(pag): Normally, this points to `item_list.end()`, meaning that
  //            everything in `item_list` is "in our past." We don't have an
  //            active item that tracks our current location because that is
  //            actively maintained by a property of the currently active
  //            file code tab.
  ItemList::iterator current_item_it{};

  // Represents our present item, and isn't added to history until something
  // changes.
  std::optional<std::pair<VariantEntity, std::optional<QString>>> next_item;

  QIcon back_icon;
  QIcon forward_icon;

  QToolButton *back_button{nullptr};
  QAction *back_action{nullptr};

  QToolButton *forward_button{nullptr};
  QAction *forward_action{nullptr};

  QShortcut *back_shortcut{nullptr};
  QShortcut *forward_shortcut{nullptr};

  inline PrivateData(const FileLocationCache &file_cache_,
                     unsigned max_history_size_)
      : file_cache(file_cache_),
        max_history_size(max_history_size_),
        current_item_it(item_list.end()) {}

  void AddToHistory(VariantEntity entity, std::optional<QString> opt_label,
                    HistoryWidget *widget);
  void NavigateBackToHistoryItem(ItemList::iterator next_item_it);
  std::optional<std::pair<VariantEntity, QString>>
    NavigateForwardToHistoryItem(ItemList::iterator next_item_it);
};

HistoryWidget::HistoryWidget(const ConfigManager &config_manager,
                             unsigned max_history_size,
                             bool install_global_shortcuts,
                             QWidget *parent)
    : QWidget(parent),
      d(new PrivateData(config_manager.FileLocationCache(), max_history_size)) {

  InitializeWidgets(parent, install_global_shortcuts);

  auto &media_manager = config_manager.MediaManager();
  OnIconsChanged(media_manager);
  connect(&media_manager, &MediaManager::IconsChanged,
          this, &HistoryWidget::OnIconsChanged);

  connect(&config_manager, &ConfigManager::IndexChanged,
          this, &HistoryWidget::OnIndexChanged);
}

HistoryWidget::~HistoryWidget(void) {}

void HistoryWidget::SetIconSize(QSize size) {
  d->back_button->setIconSize(size);
  d->forward_button->setIconSize(size);
}

void HistoryWidget::SetCurrentLocation(VariantEntity entity,
                                       std::optional<QString> opt_label) {
  if (std::holds_alternative<NotAnEntity>(entity)) {
    return;
  }

  d->next_item.reset();
  d->next_item.emplace(std::move(entity), std::move(opt_label));
}

void HistoryWidget::CommitCurrentLocationToHistory(void) {
  if (!d->next_item.has_value()) {
    return;
  }

  std::pair<VariantEntity, std::optional<QString>> ret =
      std::move(d->next_item.value());
  d->next_item.reset();
  d->AddToHistory(std::move(ret.first), std::move(ret.second), this);
  UpdateMenus();
}

void HistoryWidget::PrivateData::AddToHistory(VariantEntity original_entity,
                                              std::optional<QString> opt_label,
                                              HistoryWidget *widget) {

  if (std::holds_alternative<NotAnEntity>(original_entity)) {
    return;
  }

  VariantEntity canonical_entity = original_entity;
  VariantEntity label_entity = original_entity;

  if (std::holds_alternative<Decl>(original_entity)) {
    canonical_entity = std::get<Decl>(original_entity).canonical_declaration();
    label_entity = canonical_entity;

  // Canonicalize on the first file token. We might get updates from cursor
  // events and from open file events, so it's nice if we can make the file
  // events and cursor events "look the same" so that we don't end up with a
  // file and a file's first token beside one another in history.
  } else if (std::holds_alternative<File>(original_entity)) {
    canonical_entity = std::get<File>(original_entity).tokens().front();
  
  // If it's a compilation, then take the first token of the main source file.
  } else if (std::holds_alternative<Compilation>(original_entity)) {
    auto file = std::get<Compilation>(original_entity).main_source_file();
    canonical_entity = file.tokens().front();
    label_entity = file;
  }

  // Truncate the "previous future" history.
  if (current_item_it != item_list.end()) {
    item_list.erase(current_item_it, item_list.end());
  }

  // Cull overly old history.
  if (auto num_items = item_list.size(); num_items > max_history_size) {
    auto num_items_to_delete = static_cast<int>(num_items - max_history_size);
    auto range_start = item_list.begin();
    auto range_end = std::next(range_start, num_items_to_delete);
    item_list.erase(range_start, range_end);
  }

  // Don't add repeat items to the end.
  if (item_list.empty() ||
      EntityId(item_list.back().canonical_entity) != EntityId(canonical_entity)) {

    // If we're given a label then we're done.
    if (opt_label.has_value()) {
      item_list.emplace_back(
          std::move(original_entity), std::move(canonical_entity),
          opt_label.value());

    // If we weren't given a label, then compute a label on a background thread.
    } else {
      Item &item = item_list.emplace_back(
          std::move(original_entity), std::move(canonical_entity),
          QString("Entity %1").arg(EntityId(canonical_entity).Pack()));

      auto labeller = new HistoryLabelBuilder(
          file_cache, std::move(label_entity), item.item_id);
      labeller->setAutoDelete(true);

      connect(labeller, &HistoryLabelBuilder::LabelForItem,
              widget, &HistoryWidget::OnLabelForItem);

      QThreadPool::globalInstance()->start(labeller);
    }
  }

  current_item_it = item_list.end();
}

void HistoryWidget::InitializeWidgets(QWidget *parent,
                                      bool install_global_shortcuts) {
  d->back_action = new QAction(tr("Back"), this);
  d->forward_action = new QAction(tr("Forward"), this);

  d->back_action->setToolTip(tr(kBackButtonToolTip));
  d->forward_action->setToolTip(tr(kForwardButtonToolTip));

  d->back_button = new QToolButton(this);
  d->back_button->setPopupMode(QToolButton::MenuButtonPopup);
  d->back_button->setDefaultAction(d->back_action);

  d->forward_button = new QToolButton(this);
  d->forward_button->setPopupMode(QToolButton::MenuButtonPopup);
  d->forward_button->setDefaultAction(d->forward_action);

  connect(d->back_action, &QAction::triggered, this,
          &HistoryWidget::OnNavigateBack);

  connect(d->forward_action, &QAction::triggered, this,
          &HistoryWidget::OnNavigateForward);

  // By default, there is no history, so there is nowhere for these buttons to
  // navigate.
  d->back_button->setEnabled(false);
  d->forward_button->setEnabled(false);

  QHBoxLayout *layout = new QHBoxLayout;
  layout->setContentsMargins(0, 0, 0, 0);
  setLayout(layout);

  layout->addWidget(d->back_button);
  layout->addWidget(d->forward_button);

  // Initialize the keyboard shortcuts for the parent window
  static bool global_shortcut_installed{false};
  if (install_global_shortcuts && global_shortcut_installed) {
    throw std::logic_error("Global shortcuts can only be installed once");
  }

  global_shortcut_installed = install_global_shortcuts;

  QWidget *shortcut_parent{nullptr};
  Qt::ShortcutContext shortcut_context{};
  if (install_global_shortcuts) {
    // Since we install the keyboard shortcuts on the widget, the parent
    // parameters must always be valid
    Q_ASSERT(parent != nullptr);

    shortcut_parent = parent;
    shortcut_context = Qt::ApplicationShortcut;

  } else {
    shortcut_parent = this;
    shortcut_context = Qt::WidgetWithChildrenShortcut;
  }

  d->back_shortcut =
      new QShortcut(QKeySequence::Back, shortcut_parent, this,
                    &HistoryWidget::OnNavigateBack, shortcut_context);

  d->forward_shortcut =
      new QShortcut(QKeySequence::Forward, shortcut_parent, this,
                    &HistoryWidget::OnNavigateForward, shortcut_context);
}

void HistoryWidget::UpdateMenus(void) {
  if (d->item_list.empty()) {
    d->back_button->setEnabled(false);
    d->forward_button->setEnabled(false);
    return;
  }

  // Seems like updating the existing menus does not always work. Sometimes
  // they show up out of date when clicking the buttons.
  //
  // Create them from scratch for the time being
  for (QToolButton *button : {d->back_button, d->forward_button}) {
    if (QMenu *menu = button->menu()) {
      button->setMenu(nullptr);
      menu->deleteLater();
    }
  }

  QMenu *history_back_menu = new QMenu(tr("Previous history menu"));
  connect(history_back_menu, &QMenu::triggered, this,
          &HistoryWidget::OnNavigateBackToHistoryItem);

  QMenu *history_forward_menu = new QMenu(tr("Next history menu"));
  connect(history_forward_menu, &QMenu::triggered, this,
          &HistoryWidget::OnNavigateForwardToHistoryItem);

  int num_back_actions = 0;
  int num_forward_actions = 0;
  int item_index = 0;

  // Populate everything up to the current item in the back button sub-menu.
  std::vector<QAction *> back_history_action_list;
  for (auto item_it = d->item_list.begin(); item_it != d->current_item_it;
       ++item_it) {

    const Item &item = *item_it;
    QAction *action = new QAction(item.name);
    action->setData(item_index++);

    back_history_action_list.push_back(action);
    ++num_back_actions;
  }

  // NOTE(pag): Reverse the order of actions order so that the first added
  //            action, which will be triggered on button click, also
  //            corresponds to the most recent thing in the past.
  std::reverse(back_history_action_list.begin(),
               back_history_action_list.end());
  for (QAction *action : back_history_action_list) {
    history_back_menu->addAction(action);
  }

  // Populate everything after the current item into the forward button
  // sub-menu.
  if (d->current_item_it != d->item_list.end()) {
    for (auto item_it = std::next(d->current_item_it, 1);
         item_it != d->item_list.end(); ++item_it) {

      const Item &item = *item_it;
      QAction *action = new QAction(item.name);
      action->setData(++item_index);

      history_forward_menu->addAction(action);
      ++num_forward_actions;
    }
  }

  // Enable/disable and customize the tool tip for the backward button.
  d->back_button->setMenu(history_back_menu);
  if (num_back_actions) {
    d->back_action->setToolTip(tr("Go back to ") +
                               history_back_menu->actions().first()->text());
  } else {
    d->back_action->setToolTip(tr(kBackButtonToolTip));
  }

  // Enable/disable and customize the tool tip for the forward button.
  d->forward_button->setMenu(history_forward_menu);
  if (num_forward_actions) {
    d->forward_action->setToolTip(
        tr("Go forward to ") + history_forward_menu->actions().first()->text());
  } else {
    d->forward_action->setToolTip(tr(kForwardButtonToolTip));
  }

  // NOTE(pag): For some reason, resetting the tooltips removes the icons,
  //            so add them back in.
  d->back_button->setIcon(d->back_icon);
  d->forward_button->setIcon(d->forward_icon);

  // NOTE(pag): It seems like setting icones or tooltips also re-enables the
  //            buttons, so we possible disable them here.
  d->back_button->setEnabled(0 < num_back_actions);
  d->forward_button->setEnabled(0 < num_forward_actions);
}

void HistoryWidget::OnIconsChanged(const MediaManager &media_manager) {

  QIcon back_icon;
  back_icon.addPixmap(media_manager.Pixmap("com.trailofbits.icon.Back"),
                                           QIcon::Normal, QIcon::On);
  
  back_icon.addPixmap(media_manager.Pixmap("com.trailofbits.icon.Back",
                                           ITheme::IconStyle::DISABLED),
                                           QIcon::Disabled, QIcon::On);

  QIcon forward_icon;
  forward_icon.addPixmap(media_manager.Pixmap("com.trailofbits.icon.Forward"),
                                              QIcon::Normal, QIcon::On);
  
  forward_icon.addPixmap(media_manager.Pixmap("com.trailofbits.icon.Forward",
                                              ITheme::IconStyle::DISABLED),
                                              QIcon::Disabled, QIcon::On);

  d->back_icon = std::move(back_icon);
  d->forward_icon = std::move(forward_icon);

  d->back_button->setIcon(d->back_icon);
  d->forward_button->setIcon(d->forward_icon);
}

void HistoryWidget::OnLabelForItem(uint64_t item_id, const QString &label) {
  for (Item &item : d->item_list) {
    if (item.item_id == item_id) {
      item.name = label;
      UpdateMenus();
      return;
    }
  }
}

void HistoryWidget::OnNavigateBack(void) {
  if (d->current_item_it == d->item_list.begin()) {
    return;
  }

  QAction dummy;
  ssize_t index =
      std::distance(d->item_list.begin(), std::prev(d->current_item_it, 1));
  Q_ASSERT(0 <= index);
  dummy.setData(QVariant::fromValue(static_cast<size_t>(index)));

  OnNavigateBackToHistoryItem(&dummy);
}

void HistoryWidget::OnNavigateForward(void) {
  if (d->current_item_it == d->item_list.end()) {
    return;
  }

  QAction dummy;
  ssize_t index =
      std::distance(d->item_list.begin(), std::next(d->current_item_it, 1));
  Q_ASSERT(0 < index);
  dummy.setData(QVariant::fromValue(static_cast<size_t>(index)));
  OnNavigateForwardToHistoryItem(&dummy);
}

void HistoryWidget::OnNavigateBackToHistoryItem(QAction *action) {
  QVariant item_index_var = action->data();
  if (!item_index_var.isValid()) {
    return;
  }
  ItemList::iterator it =
      std::next(d->item_list.begin(), item_index_var.toInt());

  VariantEntity original_entity = it->original_entity;
  VariantEntity canonical_entity = it->canonical_entity;

  // If we're going back to some place in the past, and if we're starting from
  // "the present," then we need to materialize a history item representing our
  // present location so that when we go backward, we can always return to our
  // current (soon to be former) present location.
  if (d->current_item_it == d->item_list.end()) {
    auto index = std::distance(d->item_list.begin(), it);
    CommitCurrentLocationToHistory();
    it = std::next(d->item_list.begin(), index);
  }

  d->NavigateBackToHistoryItem(it);
  UpdateMenus();
  emit GoToEntity(std::move(original_entity), std::move(canonical_entity));
}

void HistoryWidget::OnNavigateForwardToHistoryItem(QAction *action) {
  QVariant item_index_var = action->data();
  if (!item_index_var.isValid()) {
    return;
  }

  ItemList::iterator it =
      std::next(d->item_list.begin(), item_index_var.toInt());

  VariantEntity original_entity = it->original_entity;
  VariantEntity canonical_entity = it->canonical_entity;

  if (auto opt_next_location = d->NavigateForwardToHistoryItem(it); opt_next_location.has_value()) {
    auto &next_location = opt_next_location.value();
    SetCurrentLocation(std::move(next_location.first), std::move(next_location.second));
  }

  UpdateMenus();
  emit GoToEntity(std::move(original_entity), std::move(canonical_entity));
}

void HistoryWidget::OnIndexChanged(const ConfigManager &config_manager) {
  d->file_cache = config_manager.FileLocationCache();
  d->item_list.clear();
  d->next_item.reset();
  d->current_item_it = d->item_list.end();
  d->back_button->setEnabled(false);
  d->forward_button->setEnabled(false);
}

void HistoryWidget::PrivateData::NavigateBackToHistoryItem(
    ItemList::iterator next_item_it) {
  Q_ASSERT(!item_list.empty());
  current_item_it = next_item_it;
}

std::optional<std::pair<VariantEntity, QString>>
HistoryWidget::PrivateData::NavigateForwardToHistoryItem(
    ItemList::iterator next_item_it) {

  Q_ASSERT(2u <= item_list.size());
  Q_ASSERT(next_item_it != item_list.begin());
  Q_ASSERT(next_item_it != item_list.end());

  // If we're back to the present, then take off the previously materialized
  // "present" value.
  std::optional<std::pair<VariantEntity, QString>> opt_next_location;

  if (std::next(next_item_it, 1) == item_list.end()) {
    auto &item = *next_item_it;
    opt_next_location = std::make_pair(std::move(item.original_entity), std::move(item.name));

    item_list.pop_back();
    current_item_it = item_list.end();

  } else {
    current_item_it = next_item_it;
  }

  return opt_next_location;
}

}  // namespace mx::gui
