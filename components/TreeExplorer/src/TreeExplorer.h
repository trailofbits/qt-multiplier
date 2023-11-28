/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/ITreeExplorer.h>
#include <multiplier/ui/IThemeManager.h>

namespace mx::gui {

//! A treeview-based implementation for the ITreeExplorer interface
class TreeExplorer final : public ITreeExplorer {
  Q_OBJECT

 public:
  //! Destructor
  virtual ~TreeExplorer() override;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Constructor
  TreeExplorer(ITreeExplorerModel *model,
               IGlobalHighlighter *global_highlighter, QWidget *parent);

  //! Called when a menu or osd action is about to be shown to screen
  void UpdateAction(QAction *action);

 private slots:
  //! Called when an item needs to be expanded
  void OnExpandAction();

  //! Called when navigating to an aliased item
  void OnGoToAction();

  //! Called when an item needs to be opened in the main window
  void OnOpenAction();

  //! Called by the theme manager when the theme is changed
  void OnThemeChange(const QPalette &palette,
                     const CodeViewTheme &code_view_theme);

  //! Called when a generator request starts
  void OnModelRequestStarted();

  //! Called when a generator request ends
  void OnModelRequestFinished();

  friend class ITreeExplorer;
};

}  // namespace mx::gui
