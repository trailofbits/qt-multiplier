/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/GUI/IEntityExplorerModel.h>
#include <multiplier/GUI/IGlobalHighlighter.h>

#include <QWidget>

namespace mx::gui {

//! The entity explorer widget
class IEntityExplorer : public QWidget {
  Q_OBJECT

 private:
  //! Disabled copy constructor
  IEntityExplorer(const IEntityExplorer &) = delete;

  //! Disabled copy assignment operator
  IEntityExplorer &operator=(const IEntityExplorer &) = delete;

 public:
  //! Factory method
  static IEntityExplorer *
  Create(IEntityExplorerModel *model, QWidget *parent = nullptr,
         IGlobalHighlighter *global_highlighter = nullptr);

  //! Constructor
  IEntityExplorer(QWidget *parent) : QWidget(parent) {}

  //! Destructor
  virtual ~IEntityExplorer() override = default;

  //! Returns the active model
  virtual IEntityExplorerModel *Model() = 0;

 signals:
  void EntityAction(RawEntityId id);
};

}  // namespace mx::gui
