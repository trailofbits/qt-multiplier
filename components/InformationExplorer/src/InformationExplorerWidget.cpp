/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/ui/IInformationExplorer.h>
#include <multiplier/ui/InformationExplorerWidget.h>

#include <QVBoxLayout>

namespace mx::gui {

struct InformationExplorerWidget::PrivateData final {
  IInformationExplorerModel *model{nullptr};
  IInformationExplorer *info_explorer{nullptr};
};

InformationExplorerWidget::~InformationExplorerWidget() {}

InformationExplorerWidget::InformationExplorerWidget(
    Index index, FileLocationCache file_location_cache,
    IGlobalHighlighter *global_highlighter, const bool &enable_history,
    QWidget *parent)
    : QWidget(parent),
      d(new PrivateData) {

  d->model =
      IInformationExplorerModel::Create(index, file_location_cache, this);

  connect(d->model, &QAbstractItemModel::modelReset, this,
          &InformationExplorerWidget::OnModelReset);

  d->info_explorer = IInformationExplorer::Create(
      d->model, this, global_highlighter, enable_history);

  auto layout = new QVBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);

  layout->addWidget(d->info_explorer);
  setLayout(layout);

  connect(d->info_explorer, &IInformationExplorer::SelectedItemChanged, this,
          &InformationExplorerWidget::SelectedItemChanged);

  connect(&IThemeManager::Get(), &IThemeManager::ThemeChanged, this,
          &InformationExplorerWidget::OnThemeChange);

  // This widget can be created way after the main window initialization.
  // If that is the case, we won't get the first theme change update. Manually
  // force an update now
  auto palette = IThemeManager::Get().GetPalette();
  auto code_view_theme = IThemeManager::Get().GetCodeViewTheme();
  OnThemeChange(palette, code_view_theme);
}

void InformationExplorerWidget::DisplayEntity(const RawEntityId &entity_id) {
  d->model->RequestEntityInformation(entity_id);
}

void InformationExplorerWidget::OnModelReset() {
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

void InformationExplorerWidget::OnThemeChange(const QPalette &,
                                              const CodeViewTheme &) {
  update();
}

}  // namespace mx::gui
