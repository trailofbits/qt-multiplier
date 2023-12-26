/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/IGlobalHighlighter.h>
#include <multiplier/ui/IThemeManager.h>

#include <multiplier/Index.h>

#include <memory>

#include <QDockWidget>

namespace mx::gui {

//! A component that wraps an InformationExplorer widget with its model
class InformationExplorerWidget final : public QWidget {
  Q_OBJECT

 public:
  //! Constructor
  InformationExplorerWidget(Index index, FileLocationCache file_location_cache,
                            IGlobalHighlighter *global_highlighter,
                            bool enable_history, QWidget *parent);

  //! Destructor
  virtual ~InformationExplorerWidget() override;

  //! Disabled copy constructor
  InformationExplorerWidget(const InformationExplorerWidget &) = delete;

  //! Disabled copy assignment operator
  InformationExplorerWidget &
  operator=(const InformationExplorerWidget &) = delete;

 private:
  //! Private widget data
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public slots:
  //! Requests the internal model to display the specified entity
  void DisplayEntity(const RawEntityId &entity_id);

 private slots:
  //! Used to update the window title
  void OnModelReset();

  //! Called by the theme manager
  void OnThemeChange(const QPalette &palette,
                     const CodeViewTheme &code_view_theme);

 signals:
  //! Forwards the internal InformationExplorer::SelectedItemChanged signal
  void SelectedItemChanged(const QModelIndex &current_index);
};

}  // namespace mx::gui
