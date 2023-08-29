/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "DockableInformationExplorer.h"

#include <multiplier/ui/IInformationExplorer.h>

namespace mx::gui {

struct DockableInformationExplorer::PrivateData final {
  IInformationExplorerModel *model{nullptr};
  IInformationExplorer *info_explorer{nullptr};
};

DockableInformationExplorer *DockableInformationExplorer::Create(
    Index index, FileLocationCache file_location_cache,
    IGlobalHighlighter *global_highlighter, const bool &enable_history,
    QWidget *parent) {

  auto obj = new DockableInformationExplorer(
      index, file_location_cache, global_highlighter, enable_history, parent);

  obj->setWidget(obj->d->info_explorer);
  obj->setAllowedAreas(Qt::AllDockWidgetAreas);
  obj->setWindowTitle(tr("Information Explorer"));

  // This widget can be created way after the main window initialization.
  // If that is the case, we won't get the first theme change update. Manually
  // force an update now
  auto palette = IThemeManager::Get().GetPalette();
  auto code_view_theme = IThemeManager::Get().GetCodeViewTheme();
  obj->OnThemeChange(palette, code_view_theme);

  return obj;
}

DockableInformationExplorer::~DockableInformationExplorer() {}

DockableInformationExplorer::DockableInformationExplorer(
    Index index, FileLocationCache file_location_cache,
    IGlobalHighlighter *global_highlighter, const bool &enable_history,
    QWidget *parent)
    : QDockWidget(parent),
      d(new PrivateData) {

  d->model =
      IInformationExplorerModel::Create(index, file_location_cache, this);

  connect(d->model, &QAbstractItemModel::modelReset, this,
          &DockableInformationExplorer::OnModelReset);

  d->info_explorer = IInformationExplorer::Create(
      d->model, this, global_highlighter, enable_history);

  connect(d->info_explorer, &IInformationExplorer::SelectedItemChanged, this,
          &DockableInformationExplorer::SelectedItemChanged);

  connect(&IThemeManager::Get(), &IThemeManager::ThemeChanged, this,
          &DockableInformationExplorer::OnThemeChange);
}

void DockableInformationExplorer::DisplayEntity(const RawEntityId &entity_id) {
  d->model->RequestEntityInformation(entity_id);
}

void DockableInformationExplorer::OnModelReset() {
  QString window_title;

  auto opt_entity_name = d->model->GetCurrentEntityName();
  if (opt_entity_name.has_value()) {
    window_title = tr("Entity info: '") + opt_entity_name.value() + "'";

  } else {
    auto entity_id = d->model->GetCurrentEntityID();
    if (entity_id != kInvalidEntityId) {
      window_title = tr("Entity info: #") + QString::number(entity_id);

    } else {
      window_title = tr("Entity info: Unknown entity)");
    }
  }

  setWindowTitle(window_title);
}

void DockableInformationExplorer::OnThemeChange(
    const QPalette &palette, const CodeViewTheme &code_view_theme) {
  // Do not spawn popups from this widget without restoring the
  // real application palette first!
  auto custom_palette = palette;
  custom_palette.setBrush(QPalette::Base,
                          QBrush(code_view_theme.default_background_color));

  setPalette(custom_palette);
  update();
}

}  // namespace mx::gui
