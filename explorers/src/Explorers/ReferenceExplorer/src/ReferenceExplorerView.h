/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/GUI/ThemeManager.h>
#include <multiplier/GUI/IGlobalHighlighter.h>
#include <multiplier/GUI/IGeneratorModel.h>

#include <QAbstractItemModel>
#include <QWidget>

namespace mx::gui {

//! An IGeneratorView view of type `ReferenceExplorer`
class ReferenceExplorerView final : public QWidget {
  Q_OBJECT

 public:
  //! Constructor
  ReferenceExplorerView(IGeneratorModel *model,
                        IGlobalHighlighter *global_highlighter,
                        QWidget *parent = nullptr);

  //! Destructor
  virtual ~ReferenceExplorerView() override;

  //! Disabled copy constructor
  ReferenceExplorerView(const ReferenceExplorerView &) = delete;

  //! Disabled copy assignment operator
  ReferenceExplorerView &operator=(const ReferenceExplorerView &) = delete;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Called when a menu or osd action is about to be shown to screen
  void UpdateAction(QAction *action);

 private slots:
  //! Called when an item needs to be expanded
  void OnExpandAction();

  //! Called when navigating to an aliased item
  void OnGoToAction();

  //! Called when an item needs to be opened in the main window
  void OnOpenAction();

  //! Called when the user wants to expand N levels deep
  void OnExpandNLevelsAction(unsigned n);

  //! Called by the theme manager when the theme is changed
  void OnThemeChange(const QPalette &palette,
                     const CodeViewTheme &code_view_theme);

  //! Called when a generator request starts
  void OnModelRequestStarted();

  //! Called when a generator request ends
  void OnModelRequestFinished();

 signals:
  //! Emitted when the selected item has changed
  void SelectedItemChanged(const QModelIndex &index);

  //! Emitted when an item has been activated using the dedicated button
  void ItemActivated(const QModelIndex &index);

  friend class IReferenceExplorerView;
};

}  // namespace mx::gui
