// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "RenameCommands.h"

#include <QObject>

namespace mx::gui {

RenameCommand::RenameCommand(NameRenamesModel *model, RenameEntry entry,
                             QString previous_name)
    : QUndoCommand(previous_name.isEmpty()
                       ? QObject::tr("Rename Entity")
                       : QObject::tr("Edit Rename")),
      model_(model),
      entry_(std::move(entry)),
      previous_name_(std::move(previous_name)) {}

void RenameCommand::redo(void) {
  model_->addRename(entry_.canonical_id, entry_.original_name,
                    entry_.new_name, entry_.kind, entry_.location,
                    entry_.all_ids);
}

void RenameCommand::undo(void) {
  if (previous_name_.isEmpty()) {
    // Was a new rename; remove it.
    model_->removeRename(entry_.canonical_id);
  } else {
    // Was an edit; restore previous name.
    model_->addRename(entry_.canonical_id, entry_.original_name,
                      previous_name_, entry_.kind, entry_.location,
                      entry_.all_ids);
  }
}

RemoveRenameCommand::RemoveRenameCommand(NameRenamesModel *model,
                                         RenameEntry entry)
    : QUndoCommand(QObject::tr("Remove Rename")),
      model_(model),
      entry_(std::move(entry)) {}

void RemoveRenameCommand::redo(void) {
  model_->removeRename(entry_.canonical_id);
}

void RemoveRenameCommand::undo(void) {
  model_->addRename(entry_.canonical_id, entry_.original_name,
                    entry_.new_name, entry_.kind, entry_.location,
                    entry_.all_ids);
}

}  // namespace mx::gui
