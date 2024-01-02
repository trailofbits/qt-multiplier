/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QWidget>
#include <optional>

namespace mx {
enum class TokenCategory : unsigned char;
}  // namespace mx
namespace mx::gui {

//! A listbox containing all the token categories
class CategoryComboBox final : public QWidget {
  Q_OBJECT

 public:
  //! Constructor
  CategoryComboBox(QWidget *parent);

  //! Destructor
  virtual ~CategoryComboBox(void) override;

  //! Resets the category back to "All", emitting a CategoryChanged signal
  void Reset(void);

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Initializes the internal widgets
  void InitializeWidgets(void);

 private slots:
  //! Called when the active item in the combobox changes
  void OnCurrentIndexChange(void);

 signals:
  //! Emitted when the category to show changes
  void CategoryChanged(std::optional<TokenCategory> new_category);
};

}  // namespace mx::gui
