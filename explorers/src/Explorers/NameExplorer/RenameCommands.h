// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QUndoCommand>

#include "NameRenamesModel.h"

namespace mx::gui {

// Undoable command for adding or editing a rename.
// If previous_name is empty, this is a new rename (undo removes it).
// If previous_name is set, this is an edit (undo restores the old name).
class RenameCommand Q_DECL_FINAL : public QUndoCommand {
 public:
  RenameCommand(NameRenamesModel *model, RenameEntry entry,
                QString previous_name = {});

  void redo(void) Q_DECL_FINAL;
  void undo(void) Q_DECL_FINAL;

 private:
  NameRenamesModel *model_;
  RenameEntry entry_;
  QString previous_name_;
};

// Undoable command for removing a rename.
class RemoveRenameCommand Q_DECL_FINAL : public QUndoCommand {
 public:
  RemoveRenameCommand(NameRenamesModel *model, RenameEntry entry);

  void redo(void) Q_DECL_FINAL;
  void undo(void) Q_DECL_FINAL;

 private:
  NameRenamesModel *model_;
  RenameEntry entry_;
};

}  // namespace mx::gui
