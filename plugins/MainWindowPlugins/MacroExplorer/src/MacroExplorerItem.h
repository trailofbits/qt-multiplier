/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/GUI/IThemeManager.h>
#include <multiplier/Types.h>
#include <optional>

#include <QPalette>
#include <QWidget>

namespace mx::gui {

class MacroExplorerItem final : public QWidget {
  Q_OBJECT

 public:
  //! Destructor
  virtual ~MacroExplorerItem() override;

  //! Disabled copy constructor
  MacroExplorerItem(const MacroExplorerItem &) = delete;

  //! Disabled copy assignment operator
  MacroExplorerItem &operator=(const MacroExplorerItem &) = delete;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Constructor
  MacroExplorerItem(
      RawEntityId entity_id, bool is_global, const QString &name_label,
      const std::optional<QString> &opt_location_label=std::nullopt,
      QWidget *parent=nullptr);

  //! Updates the button icons based on the active theme
  void UpdateIcons();

 private slots:

  //! Called when the user presses the delete button
  void onDeleteButtonPress();

  //! Called by the theme manager
  void OnThemeChange(const QPalette &palette,
                     const CodeViewTheme &code_view_theme);

 signals:
  //! Emitted when the highlight should be deleted
  void Deleted(RawEntityId entity_id);

  friend class MacroExplorer;
};

}  // namespace mx::gui
