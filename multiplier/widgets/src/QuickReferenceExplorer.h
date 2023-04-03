// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/ui/IReferenceExplorer.h>

#include <QWidget>
#include <QMimeData>

namespace mx::gui {

//! A reference explorer for context menus
class QuickReferenceExplorer final : public QWidget {
  Q_OBJECT

 public:
  //! Constructor
  QuickReferenceExplorer(
      const Index &index, const FileLocationCache &file_location_cache,
      RawEntityId entity_id,
      const IReferenceExplorerModel::ExpansionMode &expansion_mode,
      QWidget *parent = nullptr);

  //! Destructor
  virtual ~QuickReferenceExplorer() override;

  //! Disabled copy constructor
  QuickReferenceExplorer(const QuickReferenceExplorer &) = delete;

  //! Disabled copy assignment operator
  QuickReferenceExplorer &operator=(const QuickReferenceExplorer &) = delete;

 signals:
  //! Emitted when the references should be saved by the parent widget
  //! \param window_title A reference explorer could contain multiple
  //!        different things inside once they have been edited. For
  //!        this reason, we emit the window title instead of the
  //!        entity name, because that reference could have been
  //!        removed.
  void SaveAll(QMimeData *mime_data, const QString &window_title,
               const bool &as_new_tab);

  //! The forwarded PreviewableReferenceExplorer::SelectedItemChanged signal
  void SelectedItemChanged(const QModelIndex &index);

  //! The forwarded PreviewableReferenceExplorer::ItemActivated signal
  void ItemActivated(const QModelIndex &index);

 protected:
  //! Closes the widget when the escape key is pressed
  virtual void keyPressEvent(QKeyEvent *event) override;

  //! Helps determine if the widget should be restored on focus
  virtual void showEvent(QShowEvent *event) override;

  //! Helps determine if the widget should be restored on focus
  virtual void closeEvent(QCloseEvent *event) override;

  //! Used to handle window movements
  virtual bool eventFilter(QObject *obj, QEvent *event) override;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Initializes the internal widgets
  void InitializeWidgets(
      const Index &index, const FileLocationCache &file_location_cache,
      RawEntityId entity_id,
      const IReferenceExplorerModel::ExpansionMode &expansion_mode);

  //! Used to emit the SaveAll signal
  void EmitSaveSignal(const bool &as_new_tab);

  //! Used to start window dragging
  void OnTitleFrameMousePress(QMouseEvent *event);

  //! Used to move the window by moving the title frame
  void OnTitleFrameMouseMove(QMouseEvent *event);

  //! Used to stop window dragging
  void OnTitleFrameMouseRelease(QMouseEvent *event);

  //! Aborts the running request
  void CancelRunningRequest();

  //! Generate a new window name for the given entity name
  static QString GenerateWindowName(
      const QString &entity_name,
      const IReferenceExplorerModel::ExpansionMode &expansion_mode);

  //! Generate a new window name for the given entity id
  static QString GenerateWindowName(
      const RawEntityId &entity_id,
      const IReferenceExplorerModel::ExpansionMode &expansion_mode);

 private slots:
  //! Restores the widget visibility when the application gains focus
  void OnApplicationStateChange(Qt::ApplicationState state);

  //! Called when the save to active tab button is pressed
  void OnSaveAllToActiveRefExplorerButtonPress();

  //! Called when the save to new tab button is pressed
  void OnSaveAllToNewRefExplorerButtonPress();

  //! Called when the entity name resolution has finished
  void EntityNameFutureStatusChanged();
};

}  // namespace mx::gui
