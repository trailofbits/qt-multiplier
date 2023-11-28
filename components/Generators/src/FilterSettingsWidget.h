/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QAbstractItemModel>
#include <QWidget>

#include <memory>
#include <vector>

namespace mx::gui {

//! A search widget addon used to select additional filter parameters
class FilterSettingsWidget final : public QWidget {
  Q_OBJECT

 public:
  //! Constructor
  FilterSettingsWidget(QAbstractItemModel *model, QWidget *parent);

  //! Destructor
  virtual ~FilterSettingsWidget() override;

  //! Disabled copy constructor
  FilterSettingsWidget(const FilterSettingsWidget &) = delete;

  //! Disabled copy assignment operator
  FilterSettingsWidget &operator=(const FilterSettingsWidget &) = delete;

 public slots:
  //! Shows the widget, then signals ColumnFilterStateListChanged
  void Activate();

  //! Hides the widget, resets all options, then signals ColumnFilterStateListChanged
  void Deactivate();

 signals:
  //! Emitted when any of the settings have changed.
  void ColumnFilterStateListChanged(
      const std::vector<bool> &column_filter_state_list);

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Returns the column filter state list
  std::vector<bool> GetColumnFilterStateList();

  //! Initializes the internal widgets, releasing any previous layout
  void InitializeWidgets();

  //! Resets the search settings to the default values.
  void ResetCheckboxes();

  //! Emits the ColumnFilterStateListChanged signal
  void EmitColumnFilterStateListChanged();

 private slots:
  //! Use to (re)generate the filter checkboxes in the layout
  void OnModelReset(void);

  //! Emitted when any of the settings have changed.
  void OnCheckboxStateChange(int);
};

}  // namespace mx::gui
