/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "GlobalHighlighter.h"
#include "GlobalHighlighterItem.h"

#include <multiplier/ui/Icons.h>
#include <multiplier/ui/ITokenLabel.h>

#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QPixmap>
#include <QColorDialog>

namespace mx::gui {

struct GlobalHighlighterItem::PrivateData final {
  //! The main raw entity id for this highlight
  RawEntityId entity_id;

  //! The label containing the entity name
  QWidget *entity_name{nullptr};

  //! The highlight color
  QColor color;

  //! The 'delete' button
  QPushButton *delete_button{nullptr};

  //! The color button
  QPushButton *change_color_button{nullptr};
};

GlobalHighlighterItem::~GlobalHighlighterItem() {}

bool GlobalHighlighterItem::eventFilter(QObject *object, QEvent *event) {
  if (object == d->entity_name && event->type() == QEvent::MouseButtonPress) {
    emit EntityClicked(d->entity_id);
  }

  return false;
}

GlobalHighlighterItem::GlobalHighlighterItem(
    const RawEntityId &entity_id, const QString &name,
    const std::optional<Token> &opt_name_token, const QColor &color,
    QWidget *parent)
    : QWidget(parent),
      d(new PrivateData) {

  d->entity_id = entity_id;
  d->color = color;

  setContentsMargins(0, 0, 0, 0);

  auto layout = new QHBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);

  if (opt_name_token.has_value()) {
    d->entity_name = ITokenLabel::Create(opt_name_token.value());
  } else {
    d->entity_name = new QLabel(name);
  }

  d->entity_name->installEventFilter(this);
  layout->addWidget(d->entity_name);

  d->change_color_button = new QPushButton("");
  layout->addWidget(d->change_color_button);

  d->delete_button = new QPushButton(QIcon(), "");
  layout->addWidget(d->delete_button);

  auto button_size = d->delete_button->height();

  d->change_color_button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  d->change_color_button->resize(button_size, button_size);

  d->delete_button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  d->delete_button->resize(button_size, button_size);

  setLayout(layout);

  connect(d->change_color_button, &QPushButton::clicked, this,
          &GlobalHighlighterItem::onChangeColorButtonPress);

  connect(d->delete_button, &QPushButton::clicked, this,
          &GlobalHighlighterItem::onDeleteButtonPress);

  auto global_highlighter = static_cast<GlobalHighlighter *>(parent);

  connect(this, &GlobalHighlighterItem::ColorChanged, global_highlighter,
          &GlobalHighlighter::SetEntityColor);

  connect(this, &GlobalHighlighterItem::Deleted, global_highlighter,
          &GlobalHighlighter::RemoveEntity);

  connect(&IThemeManager::Get(), &IThemeManager::ThemeChanged, this,
          &GlobalHighlighterItem::OnThemeChange);

  UpdateIcons();
}

void GlobalHighlighterItem::UpdateIcons() {
  auto icon = GetIcon(":/Icons/GlobalHighlighter/Delete");
  d->delete_button->setIcon(icon);

  QPixmap color_button_icon(d->change_color_button->width(),
                            d->change_color_button->height());

  color_button_icon.fill(d->color);
  d->change_color_button->setIcon(QIcon(color_button_icon));
}

void GlobalHighlighterItem::onChangeColorButtonPress() {
  QColor new_color = QColorDialog::getColor();
  if (!new_color.isValid()) {
    return;
  }

  d->color = new_color;
  UpdateIcons();

  emit ColorChanged(d->entity_id, d->color);
}

void GlobalHighlighterItem::onDeleteButtonPress() {
  emit Deleted(d->entity_id);

  close();
  deleteLater();
}

void GlobalHighlighterItem::OnThemeChange(
    const QPalette &, const CodeViewTheme &code_view_theme) {
  UpdateIcons();

  QFont font{code_view_theme.font_name};
  setFont(font);
}

}  // namespace mx::gui
