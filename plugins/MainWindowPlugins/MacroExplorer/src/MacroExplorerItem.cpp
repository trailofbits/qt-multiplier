/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "MacroExplorer.h"
#include "MacroExplorerItem.h"

#include <multiplier/ui/Icons.h>

#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QPixmap>
#include <QColorDialog>

namespace mx::gui {

struct MacroExplorerItem::PrivateData final {
  //! The main raw entity id for this highlight
  RawEntityId entity_id;

  //! The 'delete' button
  QPushButton *delete_button{nullptr};
};

MacroExplorerItem::~MacroExplorerItem() {}

MacroExplorerItem::MacroExplorerItem(
    RawEntityId entity_id, bool is_global, const QString &name_label,
    const std::optional<QString> &opt_location_label, QWidget *parent)
    : QWidget(parent),
      d(new PrivateData) {

  d->entity_id = entity_id;

  QString label = name_label;
  if (is_global) {
    label = "<B><U>" + label + "</U></B>";
  }

  if (opt_location_label.has_value()) {
    label += "<BR /><small><i>" + opt_location_label.value() + "</i></small>";
  }

  auto layout = new QHBoxLayout();
  layout->addWidget(new QLabel(label));

  d->delete_button = new QPushButton(QIcon(), "");
  layout->addWidget(d->delete_button);

  auto button_size = d->delete_button->height();
  d->delete_button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  d->delete_button->resize(button_size, button_size);

  setLayout(layout);

  connect(d->delete_button, &QPushButton::clicked, this,
          &MacroExplorerItem::onDeleteButtonPress);

  auto explorer = static_cast<MacroExplorer *>(parent);

  connect(this, &MacroExplorerItem::Deleted, explorer,
          &MacroExplorer::RemoveMacro);

  connect(&IThemeManager::Get(), &IThemeManager::ThemeChanged, this,
          &MacroExplorerItem::OnThemeChange);

  UpdateIcons();
}

void MacroExplorerItem::UpdateIcons() {
  auto icon = GetIcon(":/Icons/MacroExplorer/Delete");
  d->delete_button->setIcon(icon);
}

void MacroExplorerItem::onDeleteButtonPress() {
  emit Deleted(d->entity_id);
  close();
  deleteLater();
}

void MacroExplorerItem::OnThemeChange(const QPalette &,
                                      const CodeViewTheme &code_view_theme) {
  UpdateIcons();

  QFont font(code_view_theme.font_name);
  setFont(font);
}

}  // namespace mx::gui
