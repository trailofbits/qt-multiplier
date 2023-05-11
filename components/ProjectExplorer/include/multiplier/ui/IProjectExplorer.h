/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/IFileTreeModel.h>

#include <QWidget>

namespace mx::gui {

//! A widget that displays the contents of an Index as a tree view
class IProjectExplorer : public QWidget {
  Q_OBJECT

 public:
  //! Factory function
  static IProjectExplorer *Create(IFileTreeModel *model,
                                  QWidget *parent = nullptr);

  //! Constructor
  IProjectExplorer(QWidget *parent) : QWidget(parent) {}

  //! Destructor
  virtual ~IProjectExplorer() override = default;

  //! Disable the copy constructor
  IProjectExplorer(const IProjectExplorer &) = delete;

  //! Disable copy assignment
  IProjectExplorer &operator=(const IProjectExplorer &) = delete;

 signals:
  //! Emitted when a file has been clicked
  void FileClicked(RawEntityId file_id, QString file_name,
                   Qt::KeyboardModifiers modifiers, Qt::MouseButtons buttons);
};

}  // namespace mx::gui
