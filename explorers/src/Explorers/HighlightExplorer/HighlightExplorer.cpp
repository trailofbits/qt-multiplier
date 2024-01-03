// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Explorers/HighlightExplorer.h>

#include <QColor>
#include <QColorDialog>

#include <multiplier/GUI/Interfaces/IModel.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/ThemeManager.h>
#include <multiplier/Index.h>
#include <vector>

#include "HighlightThemeProxy.h"

namespace mx::gui {

struct HighlightExplorer::PrivateData {
  ThemeManager &theme_manager;
  HighlightThemeProxy *proxy{nullptr};
  std::vector<RawEntityId> eids;

  inline PrivateData(ThemeManager &theme_manager_)
      : theme_manager(theme_manager_) {}
};

HighlightExplorer::~HighlightExplorer(void) {}

HighlightExplorer::HighlightExplorer(ConfigManager &config_manager,
                                     QMainWindow *parent)
    : IMainWindowPlugin(config_manager, parent),
      d(new PrivateData(config_manager.ThemeManager())) {

  connect(&config_manager, &ConfigManager::IndexChanged,
          this, &HighlightExplorer::OnIndexChanged);
}

QWidget *HighlightExplorer::CreateDockWidget(QWidget *parent) {
  (void) parent;
  return nullptr;
}

void HighlightExplorer::ActOnContextMenu(
    QMenu *menu, const QModelIndex &index) {

  d->eids.clear();
  VariantEntity entity = IModel::EntitySkipThroughTokens(index);
  if (std::holds_alternative<NotAnEntity>(entity)) {
    return;
  }

  if (std::holds_alternative<Decl>(entity)) {
    for (auto redecl : std::get<Decl>(entity).redeclarations()) {
      d->eids.push_back(redecl.id().Pack());
    }
  } else {
    d->eids.push_back(EntityId(entity).Pack());
  }

  if (d->eids.empty() || d->eids.back() == kInvalidEntityId) {
    return;
  }

  auto present = false;
  if (d->proxy) {
    present = d->proxy->color_map.count(d->eids[0]) != 0;
  }

  auto highlight_menu = new QMenu(tr("Highlights"), menu);
  menu->addMenu(highlight_menu);

  auto set_entity_highlight = new QAction(tr("Set Color"), highlight_menu);
  highlight_menu->addAction(set_entity_highlight);
  connect(set_entity_highlight, &QAction::triggered,
          this, &HighlightExplorer::SetColor);

  if (present) {
    auto remove_entity_highlight = new QAction(tr("Remove"), highlight_menu);
    highlight_menu->addAction(remove_entity_highlight);
    connect(remove_entity_highlight, &QAction::triggered,
            this, &HighlightExplorer::RemoveColor);
  }

  if (d->proxy && !d->proxy->color_map.empty()) {
    auto reset_entity_highlights = new QAction(tr("Reset"), highlight_menu);
    highlight_menu->addAction(reset_entity_highlights);
    connect(reset_entity_highlights, &QAction::triggered,
            this, &HighlightExplorer::ClearAllColors);
  }
}

void HighlightExplorer::ColorsUpdated(void) {
  d->eids.clear();
  if (!d->proxy) {
    return;
  }

  if (d->proxy->color_map.empty()) {
    d->proxy->UninstallFromOwningManager();
    d->proxy = nullptr;
  }
}

void HighlightExplorer::OnIndexChanged(const ConfigManager &) {
  ClearAllColors();
}

void HighlightExplorer::SetColor(void) {
  QColor color = QColorDialog::getColor();
  if (!color.isValid()) {
    d->eids.clear();
    return;
  }

  auto made_proxy = false;
  if (!d->proxy) {
    made_proxy = true;
    d->proxy = new HighlightThemeProxy;
  }

  for (auto eid : d->eids) {
    d->proxy->color_map.erase(eid);
    d->proxy->color_map.try_emplace(
        eid, ITheme::ContrastingColor(color), color);
  }
  
  if (made_proxy) {
    d->theme_manager.AddProxy(IThemeProxyPtr(d->proxy));
  } else {
    ColorsUpdated();
  }
}

void HighlightExplorer::RemoveColor(void) {
  if (!d->proxy) {
    return;
  }

  for (auto eid : d->eids) {
    d->proxy->color_map.erase(eid);
  }

  ColorsUpdated();
}

void HighlightExplorer::ClearAllColors(void) {
  d->proxy->color_map.clear();
  ColorsUpdated();
}

}  // namespace mx::gui
