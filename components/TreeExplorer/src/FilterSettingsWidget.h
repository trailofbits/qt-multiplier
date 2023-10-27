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

  //! Returns true if the Nth column should be filtered.
  bool FilterByColumn(int) const;

  //! Disabled copy constructor
  FilterSettingsWidget(const FilterSettingsWidget &) = delete;

  //! Disabled copy assignment operator
  FilterSettingsWidget &operator=(const FilterSettingsWidget &) = delete;

 public slots:
  //! Shows the widget, then signals FilterParametersChanged
  void Activate();

  //! Hides the widget, resets all options, then signals FilterParametersChanged
  void Deactivate();

 signals:

  //! Emitted when any of the settings have changed.
  void FilterParametersChanged(std::vector<bool>);

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Initializes the widget from the model.
  void InstallModel(QAbstractItemModel *model);

  //! Initializes the internal widgets.
  void InitializeWidgets();

  //! Resets the search settings to the default values.
  void ResetSearchSettings();

 private slots:
  void OnModelReset(void);

  //! Emitted when any of the settings have changed.
  void OnStateChange(int);
};

}  // namespace mx::gui
