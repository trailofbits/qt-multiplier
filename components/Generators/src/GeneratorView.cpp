/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "GeneratorView.h"
#include "FilterSettingsWidget.h"
#include "SearchFilterModelProxy.h"

#include <multiplier/GUI/ThemeManager.h>
#include <multiplier/GUI/Assert.h>
#include <multiplier/GUI/TreeWidget.h>

#include <QListView>
#include <QTableView>
#include <QVBoxLayout>
#include <QPushButton>
#include <QMenu>
#include <QSortFilterProxyModel>
#include <QHeaderView>
#include <QScrollBar>
#include <QKeyEvent>

#include <variant>
#include <optional>

namespace mx::gui {

namespace {

template <typename ModelView>
void InitializeModelViewCommonSettings(
    const IGeneratorView::Configuration &config, QObject *event_filter,
    ModelView *model_view) {

  // clang-format off
  static_assert(
    std::is_same<decltype(model_view), TreeWidget *>::value ||
    std::is_same<decltype(model_view), QListView *>::value ||
    std::is_same<decltype(model_view), QTableView *>::value,

    "InitializeModelViewCommonSettings can only accept: TreeWidget, QListView, QTableView"
  );
  // clang-format on

  if (!config.menu_actions.action_list.empty()) {
    model_view->setContextMenuPolicy(Qt::CustomContextMenu);
  }

  if (!config.osd_actions.action_list.empty()) {
    model_view->installEventFilter(event_filter);
    model_view->viewport()->installEventFilter(event_filter);
    model_view->viewport()->setMouseTracking(true);
  }

  if (config.item_delegate != nullptr) {
    config.item_delegate->setParent(model_view);
    model_view->setItemDelegate(config.item_delegate);
  }

  model_view->setSelectionBehavior(QAbstractItemView::SelectRows);
  model_view->setSelectionMode(QAbstractItemView::SingleSelection);

  model_view->setTextElideMode(Qt::TextElideMode::ElideRight);
  model_view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

template <typename ModelView>
void InitializeModelViewSortingSettings(ModelView *model_view) {

  // clang-format off
  static_assert(
    std::is_same<decltype(model_view), TreeWidget *>::value ||
    std::is_same<decltype(model_view), QTableView *>::value,

    "InitializeModelViewSortingSettings can only accept: TreeWidget, QTableView"
  );
  // clang-format on

  model_view->setSortingEnabled(true);
  model_view->sortByColumn(0, Qt::AscendingOrder);
}

std::optional<QModelIndex> GetModelIndexAtCurrentMousePos(
    const std::variant<std::monostate, TreeWidget *, QTableView *, QListView *>
        &view_var) {

  QAbstractItemView *model_view{nullptr};
  if (std::holds_alternative<TreeWidget *>(view_var)) {
    model_view = std::get<TreeWidget *>(view_var);

  } else if (std::holds_alternative<QTableView *>(view_var)) {
    model_view = std::get<QTableView *>(view_var);

  } else if (std::holds_alternative<QListView *>(view_var)) {
    model_view = std::get<QListView *>(view_var);

  } else {
    return std::nullopt;
  }

  auto model_view_global_pos = model_view->mapToGlobal(QPoint(0, 0));
  auto model_view_geometry =
      model_view->rect().translated(model_view_global_pos);

  if (!model_view_geometry.contains(QCursor::pos(), true)) {
    return std::nullopt;
  }

  auto local_mouse_pos = model_view->viewport()->mapFromGlobal(QCursor::pos());
  return model_view->indexAt(local_mouse_pos);
}

}  // namespace


struct GeneratorView::PrivateData final {
  QAbstractItemModel *model{nullptr};
  Configuration config;

  QSortFilterProxyModel *sort_filter_proxy_model{nullptr};

  std::variant<std::monostate, TreeWidget *, QTableView *, QListView *>
      view_var;

  std::vector<QPushButton *> osd_button_list;
  QMenu *context_menu{nullptr};
  std::optional<QModelIndex> opt_hovered_index;
};

GeneratorView::GeneratorView(QAbstractItemModel *model,
                             const Configuration &config, QWidget *parent)
    : IGeneratorView(parent),
      d(new PrivateData) {

  d->model = model;
  d->config = config;

  InitializeWidgets();
}

GeneratorView::~GeneratorView() {}

void GeneratorView::SetSelection(const QModelIndex &index) {
  QAbstractItemView *model_view{nullptr};

  switch (d->config.view_type) {
    case Configuration::ViewType::List:
      model_view = std::get<QListView *>(d->view_var);
      break;

    case Configuration::ViewType::Table:
      model_view = std::get<QTableView *>(d->view_var);
      break;

    case Configuration::ViewType::Tree:
      model_view = std::get<TreeWidget *>(d->view_var);
      break;
  }

  auto selection_model = model_view->selectionModel();

  QModelIndex mapped_index{index};
  if (d->config.enable_sort_and_filtering) {
    mapped_index = d->sort_filter_proxy_model->mapFromSource(mapped_index);
  }

  selection_model->setCurrentIndex(
      mapped_index,
      QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);

  model_view->scrollTo(mapped_index);
}


bool GeneratorView::eventFilter(QObject *obj, QEvent *event) {
  enum class EventReceiver {
    View,
    Viewport,
    Other,
  } event_receiver;

  QAbstractItemView *model_view{nullptr};

  {
    QWidget *viewport{nullptr};

    switch (d->config.view_type) {
      case Configuration::ViewType::List:
        model_view = std::get<QListView *>(d->view_var);
        viewport = model_view->viewport();
        break;

      case Configuration::ViewType::Table:
        model_view = std::get<QTableView *>(d->view_var);
        viewport = model_view->viewport();
        break;

      case Configuration::ViewType::Tree:
        model_view = std::get<TreeWidget *>(d->view_var);
        viewport = model_view->viewport();
        break;
    }

    if (obj == model_view) {
      event_receiver = EventReceiver::View;

    } else if (obj == viewport) {
      event_receiver = EventReceiver::Viewport;

    } else {
      event_receiver = EventReceiver::Other;
    }
  }

  if (event_receiver == EventReceiver::Other) {
    return false;
  }

  auto update_osd_buttons{false};
  if (event_receiver == EventReceiver::View &&
      event->type() == QEvent::KeyPress) {

    // Forward the key press event to actions that registered a matching
    // key sequence
    auto selection_model = model_view->selectionModel();
    auto selected_index_list = selection_model->selectedIndexes();
    if (!selected_index_list.isEmpty()) {
      auto mapped_index = selected_index_list[0];
      if (d->config.enable_sort_and_filtering) {
        mapped_index = d->sort_filter_proxy_model->mapToSource(mapped_index);
      }

      auto key_event = static_cast<QKeyEvent *>(event);
      auto pressed_key_sequence = QKeySequence(key_event->keyCombination());

      for (const auto &configured_actions :
           {d->config.menu_actions, d->config.osd_actions}) {

        for (const auto &action : configured_actions.action_list) {
          auto trigger_action{false};
          for (const auto &action_key_sequence : action->shortcuts()) {
            trigger_action = action_key_sequence == pressed_key_sequence;
            if (trigger_action) {
              break;
            }
          }

          if (trigger_action) {
            action->setData(mapped_index);

            if (configured_actions.update_action_callback != nullptr) {
              configured_actions.update_action_callback(action);
            }

            if (action->isEnabled()) {
              action->trigger();
            }
          }
        }
      }
      emit KeyPressedOnItem(selected_index_list[0],
                            static_cast<Qt::Key>(key_event->key()));
    }

  } else if (event->type() == QEvent::Leave ||
             event->type() == QEvent::MouseMove) {
    // It is important to also check for the Leave event, since
    // the OSD buttons could cover the viewport and cause the
    // widgets to emit it
    d->opt_hovered_index = GetModelIndexAtCurrentMousePos(d->view_var);
    update_osd_buttons = true;

  } else if (event_receiver == EventReceiver::View &&
             event->type() == QEvent::Wheel) {
    // Disable the overlay buttons while scrolling. It is hard to keep
    // them on screen due to how the scrolling event is propagated.
    auto scrolling_enabled = (model_view->horizontalScrollBar()->isVisible() ||
                              model_view->verticalScrollBar()->isVisible());

    if (scrolling_enabled) {
      d->opt_hovered_index = std::nullopt;
    }

    update_osd_buttons = true;
  }

  if (update_osd_buttons) {
    UpdateOSDButtons();
  }

  return false;
}

void GeneratorView::resizeEvent(QResizeEvent *) {
  d->opt_hovered_index = std::nullopt;
  UpdateOSDButtons();
}

void GeneratorView::focusOutEvent(QFocusEvent *) {
  d->opt_hovered_index = std::nullopt;
  UpdateOSDButtons();
}

void GeneratorView::InitializeWidgets() {
  // Setup the sort and filter proxy
  ISearchWidget *search_widget{nullptr};
  FilterSettingsWidget *filter_settings_widget{nullptr};

  if (d->config.enable_sort_and_filtering) {
    auto model_proxy = new SearchFilterModelProxy(this);
    model_proxy->setRecursiveFilteringEnabled(true);
    model_proxy->setSourceModel(d->model);
    model_proxy->setDynamicSortFilter(true);

    d->sort_filter_proxy_model = model_proxy;

    search_widget = ISearchWidget::Create(ISearchWidget::Mode::Filter, this);
    connect(search_widget, &ISearchWidget::SearchParametersChanged, this,
            &GeneratorView::OnSearchParametersChange);

    filter_settings_widget =
        new FilterSettingsWidget(d->sort_filter_proxy_model, this);

    connect(search_widget, &ISearchWidget::Activated, filter_settings_widget,
            &FilterSettingsWidget::Activate);

    connect(search_widget, &ISearchWidget::Deactivated, filter_settings_widget,
            &FilterSettingsWidget::Deactivate);

    connect(filter_settings_widget,
            &FilterSettingsWidget::ColumnFilterStateListChanged, model_proxy,
            &SearchFilterModelProxy::OnColumnFilterStateListChange);

    search_widget->Deactivate();
  }

  // Create the internal view widget
  QAbstractItemView *model_view{nullptr};
  auto model_ptr = d->sort_filter_proxy_model != nullptr
                       ? d->sort_filter_proxy_model
                       : d->model;

  connect(model_ptr, &QAbstractItemModel::modelReset, this,
          &GeneratorView::OnModelReset);

  switch (d->config.view_type) {
    case Configuration::ViewType::List: {
      auto list_view = new QListView();
      list_view->setModel(model_ptr);

      model_view = list_view;
      d->view_var = list_view;

      InitializeModelViewCommonSettings(d->config, this, list_view);

      break;
    }

    case Configuration::ViewType::Table: {
      auto table_view = new QTableView();
      table_view->setModel(model_ptr);

      model_view = table_view;
      d->view_var = table_view;

      InitializeModelViewCommonSettings(d->config, this, table_view);
      InitializeModelViewSortingSettings(table_view);

      break;
    }

    case Configuration::ViewType::Tree: {
      auto tree_view = new TreeWidget();
      tree_view->setModel(model_ptr);
      tree_view->expandAll();

      model_view = tree_view;
      d->view_var = tree_view;

      InitializeModelViewCommonSettings(d->config, this, tree_view);
      InitializeModelViewSortingSettings(tree_view);

      tree_view->setAllColumnsShowFocus(true);
      tree_view->setExpandsOnDoubleClick(false);
      tree_view->header()->setStretchLastSection(true);
      tree_view->header()->setSectionResizeMode(0,
                                                QHeaderView::ResizeToContents);

      // Use the row insertion signal to auto-expand newly inserted items
      connect(model_ptr, &QAbstractItemModel::rowsInserted, this,
              &GeneratorView::OnRowsInserted);

      break;
    }
  }

  auto selection_model = model_view->selectionModel();
  connect(selection_model, &QItemSelectionModel::currentChanged, this,
          [this](const QModelIndex &current_index) {
            if (!current_index.isValid()) {
              return;
            }

            // As a rule, always return model indexes in the scope of the model
            // we were given when constructed
            auto mapped_index{current_index};
            if (d->config.enable_sort_and_filtering) {
              mapped_index =
                  d->sort_filter_proxy_model->mapToSource(mapped_index);

              if (!mapped_index.isValid()) {
                return;
              }
            }

            emit SelectedItemChanged(mapped_index);
          });

  // Setup the internal layout
  setContentsMargins(0, 0, 0, 0);

  auto layout = new QVBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(model_view);
  layout->addStretch();

  if (d->config.enable_sort_and_filtering) {
    layout->addWidget(filter_settings_widget);
    layout->addWidget(search_widget);
  }

  setLayout(layout);

  // Initialize the osd buttons
  for (const auto &action : d->config.osd_actions.action_list) {
    auto button = new QPushButton(this);
    d->osd_button_list.push_back(button);

    button->setVisible(false);

    connect(button, &QPushButton::pressed, this, [this, action]() {
      action->trigger();
      UpdateOSDButtons();
    });
  }

  // Initialize the menu actions
  if (!d->config.menu_actions.action_list.empty()) {
    d->context_menu = new QMenu(this);

    connect(model_view, &QWidget::customContextMenuRequested, this,
            &GeneratorView::OnOpenItemContextMenu);

    for (const auto &action : d->config.menu_actions.action_list) {
      d->context_menu->addAction(action);
    }
  }
}

void GeneratorView::UpdateOSDButtons() {
  // Hide all the buttons if there's no item being hovered
  if (!d->opt_hovered_index.has_value() || d->osd_button_list.empty()) {
    for (auto &button : d->osd_button_list) {
      button->setVisible(false);
    }

    return;
  }

  const auto &index = d->opt_hovered_index.value();

  // Go through all the osd action list; we have one for each
  // button. Update the icon, text, tooltip and state. Skip
  // the ones that are currently not enabled/visible.
  std::vector<QPushButton *> active_button_list;

  // As a rule, always return model indexes in the scope of the model
  // we were given when constructed
  auto mapped_index{index};
  if (d->config.enable_sort_and_filtering) {
    mapped_index = d->sort_filter_proxy_model->mapToSource(mapped_index);
    if (!mapped_index.isValid()) {
      return;
    }
  }

  for (std::size_t i{}; i < d->config.osd_actions.action_list.size(); ++i) {
    const auto &source_action = d->config.osd_actions.action_list[i];

    source_action->setData(mapped_index);

    if (d->config.menu_actions.update_action_callback != nullptr) {
      d->config.osd_actions.update_action_callback(source_action);
    }

    auto &button = d->osd_button_list[i];
    button->setIcon(source_action->icon());
    button->setToolTip(source_action->toolTip());

    auto enable_button =
        source_action->isEnabled() && source_action->isVisible();

    button->setEnabled(enable_button);
    button->setVisible(enable_button);

    if (enable_button) {
      active_button_list.push_back(button);
    }
  }

  if (active_button_list.empty()) {
    return;
  }

  // Get the boundaries of the hovered item, and redistribute the
  // buttons on top of it
  QAbstractItemView *model_view{nullptr};
  if (std::holds_alternative<TreeWidget *>(d->view_var)) {
    model_view = std::get<TreeWidget *>(d->view_var);

  } else if (std::holds_alternative<QTableView *>(d->view_var)) {
    model_view = std::get<QTableView *>(d->view_var);

  } else if (std::holds_alternative<QListView *>(d->view_var)) {
    model_view = std::get<QListView *>(d->view_var);

  } else {
    return;
  }

  auto hovered_item_rect = model_view->visualRect(index);

  auto button_margin = hovered_item_rect.height() / 6;
  auto button_size = hovered_item_rect.height() - (button_margin * 2);
  auto button_count = static_cast<int>(active_button_list.size());
  auto button_area_width =
      (button_count * button_size) + (button_count * button_margin);

  auto current_x =
      model_view->pos().x() + model_view->width() - button_area_width;

  const auto &vertical_scrollbar = *model_view->verticalScrollBar();
  if (vertical_scrollbar.isVisible()) {
    current_x -= vertical_scrollbar.width();
  }

  auto current_y = hovered_item_rect.y() + (hovered_item_rect.height() / 2) -
                   (button_size / 2);

  auto pos = model_view->viewport()->mapToGlobal(QPoint(current_x, current_y));
  pos = mapFromGlobal(pos);

  current_x = pos.x();
  current_y = pos.y();

  for (auto *button : active_button_list) {
    button->resize(button_size, button_size);
    button->move(current_x, current_y);
    button->raise();

    current_x += button_size + button_margin;
  }
}

void GeneratorView::OnOpenItemContextMenu(const QPoint &point) {
  if (d->context_menu == nullptr) {
    return;
  }

  QModelIndex model_index;
  QPoint global_point;

  if (std::holds_alternative<TreeWidget *>(d->view_var)) {
    auto tree_view = std::get<TreeWidget *>(d->view_var);

    model_index = tree_view->indexAt(point);
    global_point = tree_view->viewport()->mapToGlobal(point);

  } else if (std::holds_alternative<QListView *>(d->view_var)) {
    auto list_view = std::get<QListView *>(d->view_var);

    model_index = list_view->indexAt(point);
    global_point = list_view->viewport()->mapToGlobal(point);

  } else if (std::holds_alternative<QTableView *>(d->view_var)) {
    auto table_view = std::get<QTableView *>(d->view_var);

    model_index = table_view->indexAt(point);
    global_point = table_view->viewport()->mapToGlobal(point);

  } else {
    throw std::logic_error(
        "Invalid internal state in GeneratorView::OnOpenItemContextMenu");
  }

  if (!model_index.isValid()) {
    return;
  }

  // As a rule, always return model indexes in the scope of the model
  // we were given when constructed
  if (d->config.enable_sort_and_filtering) {
    model_index = d->sort_filter_proxy_model->mapToSource(model_index);
    if (!model_index.isValid()) {
      return;
    }
  }

  auto show_menu{false};
  for (auto action : d->config.menu_actions.action_list) {
    action->setData(model_index);

    if (d->config.menu_actions.update_action_callback != nullptr) {
      d->config.menu_actions.update_action_callback(action);
    }

    if (!action->isEnabled() || !action->isVisible()) {
      continue;
    }

    show_menu = true;
  }

  if (!show_menu) {
    return;
  }

  d->context_menu->exec(global_point);
}

void GeneratorView::OnRowsInserted(const QModelIndex &parent, int, int) {
  if (std::holds_alternative<TreeWidget *>(d->view_var)) {
    auto &tree_view = *std::get<TreeWidget *>(d->view_var);
    tree_view.expand(parent);
  }
}

void GeneratorView::OnSearchParametersChange(
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

  d->sort_filter_proxy_model->setFilterRegularExpression(regex);
}

void GeneratorView::OnModelReset() {
  d->opt_hovered_index = std::nullopt;
}

}  // namespace mx::gui
