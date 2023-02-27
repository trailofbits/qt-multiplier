// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/Index.h>

#include <QWidget>
#include <QMimeData>

namespace mx::gui {

//! A reference explorer for context menus
class QuickReferenceExplorer final : public QWidget {
  Q_OBJECT

 public:
  //! Constructor
  QuickReferenceExplorer(mx::Index index,
                         mx::FileLocationCache file_location_cache,
                         RawEntityId entity_id, QWidget *parent = nullptr);

  //! Destructor
  virtual ~QuickReferenceExplorer() override;

  //! Disabled copy constructor
  QuickReferenceExplorer(const QuickReferenceExplorer &) = delete;

  //! Disabled copy assignment operator
  QuickReferenceExplorer &operator=(const QuickReferenceExplorer &) = delete;

 signals:
  //! Emitted when the references should be saved by the parent widget
  void SaveAll(QMimeData *mime_data, const bool &as_new_tab);

 protected:
  //! Closes the widget when the escape key is pressed
  virtual void keyPressEvent(QKeyEvent *event) override;

  //! Used to update floating widgets
  virtual void resizeEvent(QResizeEvent *event) override;

  //! Helps determine if the widget should be restored on focus
  virtual void showEvent(QShowEvent *event) override;

  //! Helps determine if the widget should be restored on focus
  virtual void closeEvent(QCloseEvent *event) override;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Initializes the internal widgets
  void InitializeWidgets(mx::Index index,
                         mx::FileLocationCache file_location_cache,
                         RawEntityId entity_id);

  //! Updates the save button positions
  void UpdateButtonPositions();

  //! Used to emit the SaveAll signal
  void EmitSaveSignal(const bool &as_new_tab);

 private slots:
  //! Restores the widget visibility when the application gains focus
  void OnApplicationStateChange(Qt::ApplicationState state);

  //! Called when the save to active tab button is pressed
  void OnSaveAllToActiveRefExplorerButtonPress();

  //! Called when the save to new tab button is pressed
  void OnSaveAllToNewRefExplorerButtonPress();
};

}  // namespace mx::gui
