/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "TreeExplorer.h"
#include "TreeExplorerItemDelegate.h"

#include <multiplier/ui/IGeneratorView.h>
#include <multiplier/ui/Icons.h>

#include <QVBoxLayout>
#include <QAction>

namespace mx::gui {

namespace {

struct OSDAndMenuActions final {
  QAction *expand{nullptr};
  QAction *go_to{nullptr};
  QAction *open{nullptr};
};

}  // namespace

struct TreeExplorer::PrivateData final {
  ITreeExplorerModel *model{nullptr};
  QAbstractProxyModel *highlighter_model_proxy{nullptr};

  IGeneratorView *generator_view{nullptr};
  OSDAndMenuActions osd_and_menu_actions;
};

TreeExplorer::~TreeExplorer() {}

TreeExplorer::TreeExplorer(ITreeExplorerModel *model,
                           IGlobalHighlighter *global_highlighter,
                           QWidget *parent)
    : ITreeExplorer(parent),
      d(new PrivateData) {

  // Install the global highlighter proxy
  d->model = model;

  d->highlighter_model_proxy = global_highlighter->CreateModelProxy(
      d->model, ITreeExplorerModel::EntityIdRole);

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
          &TreeExplorer::OnExpandAction);

  d->osd_and_menu_actions.open = new QAction(tr("Open in main window"), this);
  d->osd_and_menu_actions.open->setToolTip(
      tr("Open this entity in the main window"));
  connect(d->osd_and_menu_actions.open, &QAction::triggered, this,
          &TreeExplorer::OnOpenAction);

  d->osd_and_menu_actions.go_to = new QAction("Go to aliased entity", this);
  d->osd_and_menu_actions.go_to->setToolTip(tr("Go to this aliased entity"));
  connect(d->osd_and_menu_actions.go_to, &QAction::triggered, this,
          &TreeExplorer::OnGoToAction);

  config.menu_actions.action_list.push_back(d->osd_and_menu_actions.go_to);
  config.menu_actions.action_list.push_back(d->osd_and_menu_actions.expand);
  config.menu_actions.action_list.push_back(d->osd_and_menu_actions.open);
  config.menu_actions.update_action_callback = [this](QAction *action) {
    UpdateAction(action);
  };

  config.osd_actions = config.menu_actions;

  // Create the view and setup the layout
  d->generator_view =
      IGeneratorView::Create(d->highlighter_model_proxy, config);

  connect(d->generator_view, &IGeneratorView::SelectedItemChanged, this,
          &ITreeExplorer::SelectedItemChanged);

  setContentsMargins(0, 0, 0, 0);

  auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(d->generator_view);
  setLayout(layout);

  // Ensure that we receive theme updates so that we can update the icons
  auto &theme_manager = IThemeManager::Get();
  connect(&theme_manager, &IThemeManager::ThemeChanged, this,
          &TreeExplorer::OnThemeChange);

  OnThemeChange(theme_manager.GetPalette(), theme_manager.GetCodeViewTheme());
}

void TreeExplorer::UpdateAction(QAction *action) {
  QModelIndex index;
  if (auto variant = action->data();
      variant.isValid() && variant.canConvert<QModelIndex>()) {
    index = variant.toModelIndex();
  }

  if (!index.isValid()) {
    return;
  }

  auto enable_action{false};

  if (action == d->osd_and_menu_actions.expand) {
    auto is_duplicate{false};
    if (auto variant = index.data(ITreeExplorerModel::IsDuplicate);
        variant.isValid() && variant.canConvert<bool>()) {
      is_duplicate = variant.toBool();
    }

    if (!is_duplicate) {
      if (auto variant = index.data(ITreeExplorerModel::CanBeExpanded);
          variant.isValid() && variant.canConvert<bool>()) {
        enable_action = variant.toBool();
      }
    }

  } else if (action == d->osd_and_menu_actions.go_to) {
    if (auto variant = index.data(ITreeExplorerModel::IsDuplicate);
        variant.isValid() && variant.canConvert<bool>()) {
      enable_action = variant.toBool();
    }

  } else if (action == d->osd_and_menu_actions.open) {
    enable_action = true;
  }

  action->setEnabled(enable_action);
  action->setVisible(enable_action);
}

void TreeExplorer::OnExpandAction() {
  auto model_index_var = d->osd_and_menu_actions.expand->data();
  if (!model_index_var.isValid() ||
      !model_index_var.canConvert<QModelIndex>()) {
    return;
  }

  auto model_index = model_index_var.toModelIndex();
  d->model->Expand(model_index, 1);
}

void TreeExplorer::OnGoToAction() {
  // The the view model index
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

  d->generator_view->SetSelection(model_index);
}

void TreeExplorer::OnOpenAction() {
  auto model_index_var = d->osd_and_menu_actions.open->data();
  if (!model_index_var.isValid() ||
      !model_index_var.canConvert<QModelIndex>()) {
    return;
  }

  auto model_index = model_index_var.toModelIndex();
  emit ItemActivated(model_index);
}

void TreeExplorer::OnThemeChange(const QPalette &,
                                 const CodeViewTheme &code_view_theme) {
  QIcon open_item_icon;
  open_item_icon.addPixmap(GetPixmap(":/TreeExplorer/activate_ref_item"),
                           QIcon::Normal, QIcon::On);

  open_item_icon.addPixmap(
      GetPixmap(":/TreeExplorer/activate_ref_item", IconStyle::Disabled),
      QIcon::Disabled, QIcon::On);

  d->osd_and_menu_actions.open->setIcon(open_item_icon);

  QIcon expand_item_icon;
  expand_item_icon.addPixmap(GetPixmap(":/TreeExplorer/expand_ref_item"),
                             QIcon::Normal, QIcon::On);

  expand_item_icon.addPixmap(
      GetPixmap(":/TreeExplorer/expand_ref_item", IconStyle::Disabled),
      QIcon::Disabled, QIcon::On);

  d->osd_and_menu_actions.expand->setIcon(expand_item_icon);

  QIcon goto_item_icon;
  goto_item_icon.addPixmap(GetPixmap(":/TreeExplorer/goto_ref_item"),
                           QIcon::Normal, QIcon::On);

  goto_item_icon.addPixmap(
      GetPixmap(":/TreeExplorer/goto_ref_item", IconStyle::Disabled),
      QIcon::Disabled, QIcon::On);

  d->osd_and_menu_actions.go_to->setIcon(goto_item_icon);
}

}  // namespace mx::gui
