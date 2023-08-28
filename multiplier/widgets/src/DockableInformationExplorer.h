/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/IGlobalHighlighter.h>

#include <multiplier/Index.h>

#include <memory>

#include <QDockWidget>

namespace mx::gui {

//! A component that wraps an InformationExplorer inside a dock widget
class DockableInformationExplorer final : public QDockWidget {
  Q_OBJECT

 public:
  //! Factory function
  static DockableInformationExplorer *
  Create(Index index, FileLocationCache file_location_cache,
         IGlobalHighlighter *global_highlighter,
         const bool &enable_history = true, QWidget *parent = nullptr);

  //! Destructor
  virtual ~DockableInformationExplorer() override;

  //! Disabled copy constructor
  DockableInformationExplorer(const DockableInformationExplorer &) = delete;

  //! Disabled copy assignment operator
  DockableInformationExplorer &
  operator=(const DockableInformationExplorer &) = delete;

 private:
  //! Private widget data
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Private constructor
  DockableInformationExplorer(Index index,
                              FileLocationCache file_location_cache,
                              IGlobalHighlighter *global_highlighter,
                              const bool &enable_history, QWidget *parent);

 public slots:
  //! Requests the internal model to display the specified entity
  void DisplayEntity(const RawEntityId &entity_id);

 private slots:
  //! Used to update the window title
  void OnModelReset();

 signals:
  //! Forwards the internal InformationExplorer::SelectedItemChanged signal
  void SelectedItemChanged(const QModelIndex &current_index);
};

}  // namespace mx::gui
