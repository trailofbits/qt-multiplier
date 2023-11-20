// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include "PreviewableReferenceExplorer.h"

#include <QWidget>

namespace mx::gui {

class IGlobalHighlighter;
class IMacroExplorer;
class ITreeGenerator;

//! A reference explorer for context menus
class QuickReferenceExplorer final : public QWidget {
  Q_OBJECT

 public:
  //! Constructor
  QuickReferenceExplorer(const Index &index,
                         const FileLocationCache &file_location_cache,
                         std::shared_ptr<ITreeGenerator> generator,
                         const bool &show_code_preview,
                         IGlobalHighlighter &highlighter,
                         IMacroExplorer &macro_explorer,
                         QWidget *parent = nullptr);

  //! Destructor
  virtual ~QuickReferenceExplorer() override;

  //! Disabled copy constructor
  QuickReferenceExplorer(const QuickReferenceExplorer &) = delete;

  //! Disabled copy assignment operator
  QuickReferenceExplorer &operator=(const QuickReferenceExplorer &) = delete;

 signals:
  //! Emitted when the user asks to dock this window
  void SaveReferenceExplorer(PreviewableReferenceExplorer *reference_explorer);

  //! The forwarded PreviewableReferenceExplorer::SelectedItemChanged signal
  void SelectedItemChanged(const QModelIndex &index);

  //! The forwarded PreviewableReferenceExplorer::ItemActivated signal
  void ItemActivated(const QModelIndex &index);

  //! The forwarded PreviewableReferenceExplorer::ExtractSubtree signal
  void ExtractSubtree(const QModelIndex &index);

 protected:
  //! Closes the widget when the escape key is pressed
  virtual void keyPressEvent(QKeyEvent *event) override;

  //! Helps determine if the widget should be restored on focus
  virtual void showEvent(QShowEvent *event) override;

  //! Helps determine if the widget should be restored on focus
  virtual void closeEvent(QCloseEvent *event) override;

  //! Used to handle window movements
  virtual bool eventFilter(QObject *obj, QEvent *event) override;

  //! Used to update the size grip position
  virtual void resizeEvent(QResizeEvent *event) override;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Initializes the internal widgets
  void InitializeWidgets(const Index &index,
                         const FileLocationCache &file_location_cache,
                         std::shared_ptr<ITreeGenerator> generator,
                         const bool &show_code_preview,
                         IGlobalHighlighter &highlighter,
                         IMacroExplorer &macro_explorer);

  //! Used to start window dragging
  void OnTitleFrameMousePress(QMouseEvent *event);

  //! Used to move the window by moving the title frame
  void OnTitleFrameMouseMove(QMouseEvent *event);

  //! Used to stop window dragging
  void OnTitleFrameMouseRelease(QMouseEvent *event);

  //! Update the widget icons to match the active theme
  void UpdateIcons();

 private slots:
  //! Restores the widget visibility when the application gains focus
  void OnApplicationStateChange(Qt::ApplicationState state);

  //! Called when the save to new tab button is pressed
  void OnSaveReferenceExplorer();

  //! Called by the theme manager
  void OnThemeChange(const QPalette &palette,
                     const CodeViewTheme &code_view_theme);

  //! Called when the model resolves the new name of the tree.
  void OnTreeNameChanged();

 public slots:
  //! Enables or disables the browser mode of the inner code view
  void SetBrowserMode(const bool &enabled);
};

}  // namespace mx::gui
