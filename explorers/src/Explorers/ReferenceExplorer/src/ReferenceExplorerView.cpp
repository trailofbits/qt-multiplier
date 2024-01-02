/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "ReferenceExplorerView.h"
#include "ReferenceExplorerItemDelegate.h"

#include <multiplier/GUI/IGeneratorView.h>
#include <multiplier/GUI/Icons.h>

#include <QVBoxLayout>
#include <QAction>
#include <QLabel>
#include <QPushButton>

namespace mx::gui {

namespace {

static constexpr unsigned kMaxExpansionLevel = 9u;

struct OSDAndMenuActions final {
  QAction *expand{nullptr};
  QAction *go_to{nullptr};
  QAction *open{nullptr};

  // this is only shown in the menu
  QAction *expand_n_levels[kMaxExpansionLevel];
};

}  // namespace

struct ReferenceExplorerView::PrivateData final {
  IGeneratorModel *model{nullptr};
  QAbstractProxyModel *highlighter_model_proxy{nullptr};

  IGeneratorView *generator_view{nullptr};
  QWidget *status_widget{nullptr};
  OSDAndMenuActions osd_and_menu_actions;
};

ReferenceExplorerView::~ReferenceExplorerView() {}

ReferenceExplorerView::ReferenceExplorerView(
    IGeneratorModel *model, IGlobalHighlighter *global_highlighter,
    QWidget *parent)
    : QWidget(parent),
      d(new PrivateData) {

  // Install the global highlighter proxy
  d->model = model;

  if (global_highlighter) {
    d->highlighter_model_proxy = global_highlighter->CreateModelProxy(
        d->model, IGeneratorModel::EntityIdRole);
  }

  // Initialize the item delegate
  auto code_view_theme = ThemeManager::Get().GetCodeViewTheme();
  auto item_delegate = new ReferenceExplorerItemDelegate(code_view_theme);
  connect(&ThemeManager::Get(), &ThemeManager::ThemeChanged, item_delegate,
          &ReferenceExplorerItemDelegate::OnThemeChange);

  IGeneratorView::Configuration config;
  config.view_type = mx::gui::IGeneratorView::Configuration::ViewType::Tree;
  config.enable_sort_and_filtering = true;
  config.item_delegate = item_delegate;

  // Initialize the osd/menu actions
  d->osd_and_menu_actions.expand = new QAction(tr("Expand"), this);
  d->osd_and_menu_actions.expand->setToolTip(tr("Expand this entity"));
  connect(d->osd_and_menu_actions.expand, &QAction::triggered, this,
          &ReferenceExplorerView::OnExpandAction);

  d->osd_and_menu_actions.open = new QAction(tr("Open in main window"), this);
  d->osd_and_menu_actions.open->setToolTip(
      tr("Open this entity in the main window"));
  connect(d->osd_and_menu_actions.open, &QAction::triggered, this,
          &ReferenceExplorerView::OnOpenAction);

  d->osd_and_menu_actions.go_to = new QAction(tr("Go to aliased entity"), this);
  d->osd_and_menu_actions.go_to->setToolTip(tr("Go to this aliased entity"));
  connect(d->osd_and_menu_actions.go_to, &QAction::triggered, this,
          &ReferenceExplorerView::OnGoToAction);

  config.menu_actions.action_list.push_back(d->osd_and_menu_actions.go_to);
  config.menu_actions.action_list.push_back(d->osd_and_menu_actions.expand);
  config.menu_actions.action_list.push_back(d->osd_and_menu_actions.open);
  config.menu_actions.update_action_callback = [this](QAction *action) {
    UpdateAction(action);
  };

  config.osd_actions = config.menu_actions;

  for (auto i = 1u; i <= kMaxExpansionLevel; ++i) {
    QAction *&action = d->osd_and_menu_actions.expand_n_levels[i - 1u];
    action = new QAction(tr("Expand &%1 levels").arg(i), this);

    //! \todo There's a Qt 6.x bug that prevents the &3 from working correctly, so
    //!       for now we have to set the shortcut explicitly
    action->setShortcut(static_cast<Qt::Key>(Qt::Key_0 + i));
    action->setToolTip(tr("Expands this entity for three levels"));

    connect(action, &QAction::triggered,
            [i, this](void) {
              OnExpandNLevelsAction(i);
            });

    // Keep these only in the menu
    config.menu_actions.action_list.push_back(action);
  }

  // Create the view
  if (d->highlighter_model_proxy) {
    d->generator_view =
        IGeneratorView::Create(d->highlighter_model_proxy, config);
  } else {
    d->generator_view = IGeneratorView::Create(d->model, config);
  }

  connect(d->generator_view, &IGeneratorView::SelectedItemChanged, this,
          &ReferenceExplorerView::SelectedItemChanged);

  // Create the status widget, which is used to cancel updates
  d->status_widget = new QWidget();
  d->status_widget->setVisible(false);

  auto status_widget_layout = new QHBoxLayout();
  status_widget_layout->setContentsMargins(0, 0, 0, 0);

  status_widget_layout->addWidget(new QLabel(tr("Updating..."), this));
  status_widget_layout->addStretch();

  auto cancel_button = new QPushButton(tr("Cancel"), this);
  status_widget_layout->addWidget(cancel_button);

  connect(cancel_button, &QPushButton::pressed, d->model,
          &IGeneratorModel::CancelRunningRequest);

  connect(d->model, &IGeneratorModel::RequestStarted, this,
          &ReferenceExplorerView::OnModelRequestStarted);

  connect(d->model, &IGeneratorModel::RequestFinished, this,
          &ReferenceExplorerView::OnModelRequestFinished);

  d->status_widget->setLayout(status_widget_layout);

  // Setup the layout
  setContentsMargins(0, 0, 0, 0);

  auto layout = new QVBoxLayout;
  layout->setContentsMargins(0, 0, 0, 0);

  d->generator_view->setSizePolicy(QSizePolicy::Expanding,
                                   QSizePolicy::Expanding);

  layout->addWidget(d->generator_view);
  layout->addStretch();
  layout->addWidget(d->status_widget);
  setLayout(layout);

  // Ensure that we receive theme updates so that we can update the icons
  auto &theme_manager = ThemeManager::Get();
  connect(&theme_manager, &ThemeManager::ThemeChanged, this,
          &ReferenceExplorerView::OnThemeChange);

  OnThemeChange(theme_manager.GetPalette(), theme_manager.GetCodeViewTheme());
}

void ReferenceExplorerView::UpdateAction(QAction *action) {
  QModelIndex index;
  if (auto variant = action->data();
      variant.isValid() && variant.canConvert<QModelIndex>()) {
    index = variant.toModelIndex();
  }

  if (!index.isValid()) {
    return;
  }

  if (action == d->osd_and_menu_actions.open) {
    action->setEnabled(true);
    action->setVisible(true);
    return;
  }

  auto is_duplicate{false};
  if (auto variant = index.data(IGeneratorModel::IsDuplicate);
      variant.isValid() && variant.canConvert<bool>()) {
    is_duplicate = variant.toBool();
  }

  if (action == d->osd_and_menu_actions.go_to) {
    action->setEnabled(is_duplicate);
    action->setVisible(is_duplicate);
    return;
  }

  // All other actions are expansion actions.

  // If this is a duplicate, then disable the expand action.
  if (is_duplicate) {
    action->setEnabled(false);
    action->setVisible(false);
    return;
  }

  auto can_expand{false};
  if (auto variant = index.data(IGeneratorModel::CanBeExpanded);
      variant.isValid() && variant.canConvert<bool>()) {
    can_expand = variant.toBool();
  }

  for (auto expand_action : d->osd_and_menu_actions.expand_n_levels) {
    if (action == expand_action) {
      action->setEnabled(true);
      action->setVisible(true);
    }
  }

  if (can_expand) {
    return;
  }


  // If this has already been expanded, then disable the expand action
  // for depth 1.
  auto disable = action == d->osd_and_menu_actions.expand ||
                 action == d->osd_and_menu_actions.expand_n_levels[0];
  action->setEnabled(!disable);
  action->setVisible(!disable);
}

void ReferenceExplorerView::OnExpandAction() {
  auto model_index_var = d->osd_and_menu_actions.expand->data();
  if (!model_index_var.isValid() ||
      !model_index_var.canConvert<QModelIndex>()) {
    return;
  }

  auto model_index = model_index_var.toModelIndex();
  d->model->Expand(model_index, 1);
}

void ReferenceExplorerView::OnGoToAction() {
  // Take the view model index
  auto model_index_var = d->osd_and_menu_actions.go_to->data();
  if (!model_index_var.isValid() ||
      !model_index_var.canConvert<QModelIndex>()) {
    return;
  }

  auto model_index = model_index_var.toModelIndex();

  // The view is using the global highlighter proxy model
  // but we need to access the original model now. Do the
  // mapping
  model_index = d->highlighter_model_proxy->mapToSource(model_index);
  if (!model_index.isValid()) {
    return;
  }

  // Find the index of the aliased entity and update the
  // tree view selection
  model_index = d->model->Deduplicate(model_index);
  if (!model_index.isValid()) {
    qDebug() << "Invalid model index at line" << __LINE__;
    return;
  }

  // Map this back to the model used by the view
  model_index = d->highlighter_model_proxy->mapFromSource(model_index);
  if (!model_index.isValid()) {
    return;
  }

  d->generator_view->SetSelection(model_index);
}

void ReferenceExplorerView::OnOpenAction() {
  // Take the view model index
  auto model_index_var = d->osd_and_menu_actions.open->data();
  if (!model_index_var.isValid() ||
      !model_index_var.canConvert<QModelIndex>()) {
    return;
  }

  auto model_index = model_index_var.toModelIndex();
  emit ItemActivated(model_index);
}

void ReferenceExplorerView::OnExpandNLevelsAction(unsigned n) {
  auto model_index_var = d->osd_and_menu_actions.expand_n_levels[n - 1u]->data();
  if (!model_index_var.isValid() ||
      !model_index_var.canConvert<QModelIndex>()) {
    return;
  }

  auto model_index = model_index_var.toModelIndex();
  d->model->Expand(model_index, n);
}

void ReferenceExplorerView::OnThemeChange(
    const QPalette &, const CodeViewTheme &) {
  QIcon open_item_icon;
  open_item_icon.addPixmap(GetPixmap(":/ReferenceExplorer/activate_ref_item"),
                           QIcon::Normal, QIcon::On);

  open_item_icon.addPixmap(
      GetPixmap(":/ReferenceExplorer/activate_ref_item", IconStyle::Disabled),
      QIcon::Disabled, QIcon::On);

  d->osd_and_menu_actions.open->setIcon(open_item_icon);

  QIcon expand_item_icon;
  expand_item_icon.addPixmap(GetPixmap(":/ReferenceExplorer/expand_ref_item"),
                             QIcon::Normal, QIcon::On);

  expand_item_icon.addPixmap(
      GetPixmap(":/ReferenceExplorer/expand_ref_item", IconStyle::Disabled),
      QIcon::Disabled, QIcon::On);

  d->osd_and_menu_actions.expand->setIcon(expand_item_icon);

  QIcon goto_item_icon;
  goto_item_icon.addPixmap(GetPixmap(":/ReferenceExplorer/goto_ref_item"),
                           QIcon::Normal, QIcon::On);

  goto_item_icon.addPixmap(
      GetPixmap(":/ReferenceExplorer/goto_ref_item", IconStyle::Disabled),
      QIcon::Disabled, QIcon::On);

  d->osd_and_menu_actions.go_to->setIcon(goto_item_icon);

  for (auto i = 1u; i <= kMaxExpansionLevel; ++i) {
    QString icon_path = QString(":/ReferenceExplorer/expand_%1_ref_item").arg(i);

    QIcon expand_icon;
    expand_icon.addPixmap(GetPixmap(icon_path), QIcon::Normal, QIcon::On);
    expand_icon.addPixmap(GetPixmap(icon_path, IconStyle::Disabled),
                          QIcon::Disabled, QIcon::On);

    d->osd_and_menu_actions.expand_n_levels[i - 1u]->setIcon(expand_icon);
  }
}

void ReferenceExplorerView::OnModelRequestStarted() {
  d->status_widget->setVisible(true);
}

void ReferenceExplorerView::OnModelRequestFinished() {
  d->status_widget->setVisible(false);
}

}  // namespace mx::gui
