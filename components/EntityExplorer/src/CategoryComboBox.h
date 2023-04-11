/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/Entities/TokenCategory.h>

#include <QWidget>

namespace mx::gui {

//! A listbox containing all the token categories
class CategoryComboBox final : public QWidget {
  Q_OBJECT

 public:
  //! Constructor
  CategoryComboBox(QWidget *parent);

  //! Destructor
  virtual ~CategoryComboBox() override;

  //! Resets the category back to "All", emitting a CategoryChanged signal
  void Reset();

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Initializes the internal widgets
  void InitializeWidgets();

 private slots:
  //! Called when the active item in the combobox changes
  void OnCurrentIndexChange();

 signals:
  //! Emitted when the category to show changes
  void CategoryChanged(const std::optional<TokenCategory> &opt_token_category);
};

}  // namespace mx::gui
