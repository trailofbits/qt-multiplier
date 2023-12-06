/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/IThemeManager.h>

#include <multiplier/Frontend/Token.h>

#include <QWidget>

namespace mx::gui {

class GlobalHighlighterItem final : public QWidget {
  Q_OBJECT

 public:
  //! Destructor
  virtual ~GlobalHighlighterItem() override;

  //! Disabled copy constructor
  GlobalHighlighterItem(const GlobalHighlighterItem &) = delete;

  //! Disabled copy assignment operator
  GlobalHighlighterItem &operator=(const GlobalHighlighterItem &) = delete;

 protected:
  //! Used to make the label widgets clickable
  virtual bool eventFilter(QObject *object, QEvent *event) override;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Constructor
  GlobalHighlighterItem(const RawEntityId &entity_id, const QString &name,
                        const std::optional<TokenRange> &opt_name_tokens,
                        const QColor &color, QWidget *parent);

  //! Updates the button icons based on the active theme
  void UpdateIcons();

 private slots:
  //! Called when the user changes the entity highlight color
  void onChangeColorButtonPress();

  //! Called when the user presses the delete button
  void onDeleteButtonPress();

  //! Called by the theme manager
  void OnThemeChange(const QPalette &palette,
                     const CodeViewTheme &code_view_theme);

 signals:
  //! Emitted when the highlight color changes
  void ColorChanged(const RawEntityId &entity_id, const QColor &color);

  //! Emitted when the highlight should be deleted
  void Deleted(const RawEntityId &entity_id);

  //! Emitted when this entity is activated
  void EntityClicked(const RawEntityId &entity_id);

  friend class GlobalHighlighter;
};

}  // namespace mx::gui
