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

//! A search widget addon used to select additional filter parameters.
class FilterSettingsWidget final : public QWidget {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  //! Constructor
  FilterSettingsWidget(QAbstractItemModel *model, QWidget *parent = nullptr);

  //! Destructor
  virtual ~FilterSettingsWidget() override;

  //! Disabled copy constructor
  FilterSettingsWidget(const FilterSettingsWidget &) = delete;

  //! Disabled copy assignment operator
  FilterSettingsWidget &operator=(const FilterSettingsWidget &) = delete;

 public slots:
  //! Shows the widget, then signals ColumnFilterStateListChanged
  void Activate(void);

  //! Hides the widget, resets all options, then signals
  //! `ColumnFilterStateListChanged`
  void Deactivate(void);

 signals:
  //! Emitted when any of the settings have changed.
  void ColumnFilterStateListChanged(
      const std::vector<bool> &column_filter_state_list);

 private:
  //! Returns the column filter state list
  std::vector<bool> GetColumnFilterStateList(void);

  //! Initializes the internal widgets, releasing any previous layout
  void InitializeWidgets(void);

  //! Resets the search settings to the default values.
  void ResetCheckboxes(void);

  //! Emits the ColumnFilterStateListChanged signal
  void EmitColumnFilterStateListChanged(void);

 private slots:
  //! Use to (re)generate the filter checkboxes in the layout
  void OnModelReset(void);

  //! Emitted when any of the settings have changed.
  void OnCheckboxStateChange(int);
};

}  // namespace mx::gui
