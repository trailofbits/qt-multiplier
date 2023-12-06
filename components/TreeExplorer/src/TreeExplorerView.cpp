/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "TreeExplorerView.h"
#include "TreeExplorerItemDelegate.h"

#include <multiplier/ui/IGeneratorView.h>
#include <multiplier/ui/Icons.h>

#include <QVBoxLayout>
#include <QAction>
#include <QLabel>
#include <QPushButton>

namespace mx::gui {

namespace {

struct OSDAndMenuActions final {
  QAction *expand{nullptr};
  QAction *go_to{nullptr};
  QAction *open{nullptr};

  // this is only shown in the menu
  QAction *expand_three_levels{nullptr};
  QAction *expand_five_levels{nullptr};
  QAction *extract_subtree{nullptr};
};

}  // namespace

struct TreeExplorerView::PrivateData final {
  IGeneratorModel *model{nullptr};
  QAbstractProxyModel *highlighter_model_proxy{nullptr};

  IGeneratorView *generator_view{nullptr};
  QWidget *status_widget{nullptr};
  OSDAndMenuActions osd_and_menu_actions;
};

TreeExplorerView::~TreeExplorerView() {}

TreeExplorerView::TreeExplorerView(IGeneratorModel *model,
                                   IGlobalHighlighter *global_highlighter,
                                   QWidget *parent)
    : ITreeExplorerView(parent),
      d(new PrivateData) {

  // Install the global highlighter proxy
  d->model = model;

  d->highlighter_model_proxy = global_highlighter->CreateModelProxy(
      d->model, IGeneratorModel::EntityIdRole);

  // Initialize the item delegate
  auto code_view_theme = IThemeManager::Get().GetCodeViewTheme();
  auto item_delegate = new TreeExplorerItemDelegate(code_view_theme);
  connect(&IThemeManager::Get(), &IThemeManager::ThemeChanged, item_delegate,
          &TreeExplorerItemDelegate::OnThemeChange);

  IGeneratorView::Configuration config;
  config.view_type = mx::gui::IGeneratorView::Configuration::ViewType::Tree;
  config.enable_sort_and_filtering = true;
  config.item_delegate = item_delegate;

  // Initialize the osd/menu actions
  d->osd_and_menu_actions.expand = new QAction(tr("Expand"), this);
  d->osd_and_menu_actions.expand->setToolTip(tr("Expand this entity"));
  connect(d->osd_and_menu_actions.expand, &QAction::triggered, this,
          &TreeExplorerView::OnExpandAction);

  d->osd_and_menu_actions.open = new QAction(tr("Open in main window"), this);
  d->osd_and_menu_actions.open->setToolTip(
      tr("Open this entity in the main window"));
  connect(d->osd_and_menu_actions.open, &QAction::triggered, this,
          &TreeExplorerView::OnOpenAction);

  d->osd_and_menu_actions.go_to = new QAction(tr("Go to aliased entity"), this);
  d->osd_and_menu_actions.go_to->setToolTip(tr("Go to this aliased entity"));
  connect(d->osd_and_menu_actions.go_to, &QAction::triggered, this,
          &TreeExplorerView::OnGoToAction);

  config.menu_actions.action_list.push_back(d->osd_and_menu_actions.go_to);
  config.menu_actions.action_list.push_back(d->osd_and_menu_actions.expand);
  config.menu_actions.action_list.push_back(d->osd_and_menu_actions.open);
  config.menu_actions.update_action_callback = [this](QAction *action) {
    UpdateAction(action);
  };

  config.osd_actions = config.menu_actions;

  // Keep these only in the menu
  d->osd_and_menu_actions.expand_three_levels =
      new QAction(tr("Expand &3 levels"), this);

  //! \todo There's a Qt 6.x bug that prevents the &3 from working correctly, so
  //!       for now we have to set the shortcut explicitly
  d->osd_and_menu_actions.expand_three_levels->setShortcut(Qt::Key_3);
  d->osd_and_menu_actions.expand_three_levels->setToolTip(
      tr("Expands this entity for three levels"));

  connect(d->osd_and_menu_actions.expand_three_levels, &QAction::triggered,
          this, &TreeExplorerView::OnExpandThreeLevelsAction);

  config.menu_actions.action_list.push_back(
      d->osd_and_menu_actions.expand_three_levels);

  d->osd_and_menu_actions.expand_five_levels =
      new QAction(tr("Expand &5 levels"), this);

  //! \todo There's a Qt 6.x bug that prevents the &3 from working correctly, so
  //!       for now we have to set the shortcut explicitly
  d->osd_and_menu_actions.expand_five_levels->setShortcut(Qt::Key_5);
  d->osd_and_menu_actions.expand_five_levels->setToolTip(
      tr("Expands this entity for five levels"));

  connect(d->osd_and_menu_actions.expand_five_levels, &QAction::triggered, this,
          &TreeExplorerView::OnExpandFiveLevelsAction);

  config.menu_actions.action_list.push_back(
      d->osd_and_menu_actions.expand_five_levels);

  d->osd_and_menu_actions.extract_subtree =
      new QAction(tr("Extract subtree"), this);

  d->osd_and_menu_actions.extract_subtree->setToolTip(
      tr("Extracts the selected subtree"));

  connect(d->osd_and_menu_actions.extract_subtree, &QAction::triggered, this,
          &TreeExplorerView::OnExtractSubtreeAction);

  config.menu_actions.action_list.push_back(
      d->osd_and_menu_actions.extract_subtree);

  // Create the view
  d->generator_view =
      IGeneratorView::Create(d->highlighter_model_proxy, config);

  connect(d->generator_view, &IGeneratorView::SelectedItemChanged, this,
          &ITreeExplorerView::SelectedItemChanged);

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
          &TreeExplorerView::OnModelRequestStarted);

  connect(d->model, &IGeneratorModel::RequestFinished, this,
          &TreeExplorerView::OnModelRequestFinished);

  d->status_widget->setLayout(status_widget_layout);

  // Setup the layout
  setContentsMargins(0, 0, 0, 0);

  auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(d->generator_view);
  layout->addWidget(d->status_widget);
  setLayout(layout);

  // Ensure that we receive theme updates so that we can update the icons
  auto &theme_manager = IThemeManager::Get();
  connect(&theme_manager, &IThemeManager::ThemeChanged, this,
          &TreeExplorerView::OnThemeChange);

  OnThemeChange(theme_manager.GetPalette(), theme_manager.GetCodeViewTheme());
}

void TreeExplorerView::UpdateAction(QAction *action) {
  QModelIndex index;
  if (auto variant = action->data();
      variant.isValid() && variant.canConvert<QModelIndex>()) {
    index = variant.toModelIndex();
  }

  if (!index.isValid()) {
    return;
  }

  auto enable_action{false};

  if (action == d->osd_and_menu_actions.expand ||
      action == d->osd_and_menu_actions.expand_three_levels ||
      action == d->osd_and_menu_actions.expand_five_levels) {
    auto is_duplicate{false};
    if (auto variant = index.data(IGeneratorModel::IsDuplicate);
        variant.isValid() && variant.canConvert<bool>()) {
      is_duplicate = variant.toBool();
    }

    if (!is_duplicate) {
      if (auto variant = index.data(IGeneratorModel::CanBeExpanded);
          variant.isValid() && variant.canConvert<bool>()) {
        enable_action = variant.toBool();
      }
    }

  } else if (action == d->osd_and_menu_actions.go_to) {
    if (auto variant = index.data(IGeneratorModel::IsDuplicate);
        variant.isValid() && variant.canConvert<bool>()) {
      enable_action = variant.toBool();
    }

  } else if (action == d->osd_and_menu_actions.open ||
             action == d->osd_and_menu_actions.extract_subtree) {
    enable_action = true;
  }

  action->setEnabled(enable_action);
  action->setVisible(enable_action);
}

void TreeExplorerView::OnExpandAction() {
  auto model_index_var = d->osd_and_menu_actions.expand->data();
  if (!model_index_var.isValid() ||
      !model_index_var.canConvert<QModelIndex>()) {
    return;
  }

  auto model_index = model_index_var.toModelIndex();
  d->model->Expand(model_index, 1);
}

void TreeExplorerView::OnGoToAction() {
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

void TreeExplorerView::OnOpenAction() {
  // Take the view model index
  auto model_index_var = d->osd_and_menu_actions.open->data();
  if (!model_index_var.isValid() ||
      !model_index_var.canConvert<QModelIndex>()) {
    return;
  }

  auto model_index = model_index_var.toModelIndex();
  emit ItemActivated(model_index);
}

void TreeExplorerView::OnExtractSubtreeAction() {
  // Take the view model index
  auto model_index_var = d->osd_and_menu_actions.extract_subtree->data();
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

  emit ExtractSubtree(model_index);
}

void TreeExplorerView::OnExpandThreeLevelsAction() {
  auto model_index_var = d->osd_and_menu_actions.expand_three_levels->data();
  if (!model_index_var.isValid() ||
      !model_index_var.canConvert<QModelIndex>()) {
    return;
  }

  auto model_index = model_index_var.toModelIndex();
  d->model->Expand(model_index, 3);
}

void TreeExplorerView::OnExpandFiveLevelsAction() {
  auto model_index_var = d->osd_and_menu_actions.expand_five_levels->data();
  if (!model_index_var.isValid() ||
      !model_index_var.canConvert<QModelIndex>()) {
    return;
  }

  auto model_index = model_index_var.toModelIndex();
  d->model->Expand(model_index, 5);
}

void TreeExplorerView::OnThemeChange(const QPalette &,
                                     const CodeViewTheme &code_view_theme) {
  QIcon open_item_icon;
  open_item_icon.addPixmap(GetPixmap(":/TreeExplorerView/activate_ref_item"),
                           QIcon::Normal, QIcon::On);

  open_item_icon.addPixmap(
      GetPixmap(":/TreeExplorerView/activate_ref_item", IconStyle::Disabled),
      QIcon::Disabled, QIcon::On);

  d->osd_and_menu_actions.open->setIcon(open_item_icon);

  QIcon expand_item_icon;
  expand_item_icon.addPixmap(GetPixmap(":/TreeExplorerView/expand_ref_item"),
                             QIcon::Normal, QIcon::On);

  expand_item_icon.addPixmap(
      GetPixmap(":/TreeExplorerView/expand_ref_item", IconStyle::Disabled),
      QIcon::Disabled, QIcon::On);

  d->osd_and_menu_actions.expand->setIcon(expand_item_icon);

  QIcon goto_item_icon;
  goto_item_icon.addPixmap(GetPixmap(":/TreeExplorerView/goto_ref_item"),
                           QIcon::Normal, QIcon::On);

  goto_item_icon.addPixmap(
      GetPixmap(":/TreeExplorerView/goto_ref_item", IconStyle::Disabled),
      QIcon::Disabled, QIcon::On);

  d->osd_and_menu_actions.go_to->setIcon(goto_item_icon);

  QIcon extract_subtree_icon;
  extract_subtree_icon.addPixmap(
      GetPixmap(":/TreeExplorerView/extract_subtree"), QIcon::Normal,
      QIcon::On);

  extract_subtree_icon.addPixmap(
      GetPixmap(":/TreeExplorerView/extract_subtree", IconStyle::Disabled),
      QIcon::Disabled, QIcon::On);

  d->osd_and_menu_actions.extract_subtree->setIcon(extract_subtree_icon);

  QIcon expand_3_item_icon;
  expand_3_item_icon.addPixmap(
      GetPixmap(":/TreeExplorerView/expand_3_ref_item"), QIcon::Normal,
      QIcon::On);

  expand_3_item_icon.addPixmap(
      GetPixmap(":/TreeExplorerView/expand_3_ref_item", IconStyle::Disabled),
      QIcon::Disabled, QIcon::On);

  d->osd_and_menu_actions.expand_three_levels->setIcon(expand_3_item_icon);

  QIcon expand_5_item_icon;
  expand_5_item_icon.addPixmap(
      GetPixmap(":/TreeExplorerView/expand_5_ref_item"), QIcon::Normal,
      QIcon::On);

  expand_5_item_icon.addPixmap(
      GetPixmap(":/TreeExplorerView/expand_5_ref_item", IconStyle::Disabled),
      QIcon::Disabled, QIcon::On);

  d->osd_and_menu_actions.expand_five_levels->setIcon(expand_5_item_icon);
}

void TreeExplorerView::OnModelRequestStarted() {
  d->status_widget->setVisible(true);
}

void TreeExplorerView::OnModelRequestFinished() {
  d->status_widget->setVisible(false);
}

}  // namespace mx::gui
