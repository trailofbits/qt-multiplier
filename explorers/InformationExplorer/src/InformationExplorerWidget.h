/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <memory>
#include <multiplier/Types.h>

#include <QDockWidget>
#include <QPalette>

namespace mx {
class Index;
class FileLocationCache;
}  // namespace mx
namespace mx::gui {

class CodeViewTheme;
class IGlobalHighlighter;
class InformationExplorerModel;
class InformationExplorer;

//! A component that wraps an InformationExplorer widget with its model
class InformationExplorerWidget final : public QWidget {
  Q_OBJECT

  // TODO(pag): Should either of these be `std::unique_ptr`?
  InformationExplorerModel * const model;
  InformationExplorer * const info_explorer;

 public:
  //! Constructor
  InformationExplorerWidget(const Index &index,
                            const FileLocationCache &file_location_cache,
                            IGlobalHighlighter *global_highlighter,
                            bool enable_history, QWidget *parent);

  //! Destructor
  virtual ~InformationExplorerWidget(void);

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
