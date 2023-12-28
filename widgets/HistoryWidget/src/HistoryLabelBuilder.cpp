/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "HistoryLabelBuilder.h"

#include <QDebug>

#include <filesystem>
#include <multiplier/Index.h>
#include <multiplier/GUI/Util.h>

namespace mx::gui {

HistoryLabelBuilder::~HistoryLabelBuilder(void) {}

//! Formulate a nice label for the history item associated with `entity`. This
//! label is shown in the back/forward drop-down menus beside the back/forward
//! history navigation buttons.
void HistoryLabelBuilder::run(void) {
  std::optional<QString> entity_label;
  std::optional<QString> in_label;
  QString line_col_label;
  QString file_label;

  if (std::holds_alternative<NotAnEntity>(entity)) {
    return;
  }

  Token file_loc;

  std::optional<File> maybe_file = File::containing(entity);
  if (maybe_file.has_value()) {
    file_loc = FirstFileToken(entity);

    // Choose the first path associated with the containing file.
    for (std::filesystem::path path : maybe_file->paths()) {
      file_label = QString::fromStdString(path.filename().generic_string());
      break;
    }

    // If the entity isn't a file, then formulate a `:line:column` suffix to
    // append to the file name. We ignore the file case, because it would be
    // noise to add in `:1:1`.
    if (!std::holds_alternative<File>(entity)) {
      if (auto maybe_line_col = file_loc.location(file_cache);
          maybe_line_col && !(maybe_line_col->first == 1u &&
                              maybe_line_col->second == 1u)) {
        line_col_label = ":" + QString::number(maybe_line_col->first) +
                         ":" + QString::number(maybe_line_col->second);
      }
    }
  }

  // Try to find a named entity that contains `entity`.
  if (VariantEntity containing_entity = NamedDeclContaining(entity);
      IdOfEntity(containing_entity) != IdOfEntity(entity)) {
    in_label = NameOfEntityAsString(containing_entity);
  }

  if (std::holds_alternative<Token>(entity)) {

    // Due to the nature of cursor setting in code views, it's possible that
    // our history stores more token IDs rather than entity IDs. If we can match
    // a token to be the "location" of the an entity, then use that entity's
    // name in our label.
    if (VariantEntity related_entity = std::get<Token>(entity).related_entity();
        file_loc == FirstFileToken(related_entity)) {
      entity_label = NameOfEntityAsString(related_entity);
    }

  } else if (std::holds_alternative<Decl>(entity)) {
    entity_label = NameOfEntityAsString(entity);

  } else if (std::holds_alternative<File>(entity)) {

  } else {
    return;
  }

  // Put all the sub-labels together. We have a few different forms:
  //
  //      NAME
  //      FILE
  //      FILE:LINE:COL
  //      NAME at FILE:LINE:COL
  //      FILE:LINE:COL in NAME
  //      NAME1 at FILE:LINE:COL in NAME2
  QString label = file_label + line_col_label;
  if (entity_label.has_value() && !entity_label->isEmpty()) {
    if (!label.isEmpty()) {
      label = tr(" at ") + label;
    }
    if (in_label.has_value() && !in_label->isEmpty() &&
        in_label.value() != entity_label.value()) {
      label = entity_label.value() + label + tr(" in ") +
              in_label.value();
    } else {
      label = entity_label.value() + label;
    }
  } else if (in_label.has_value() && !in_label->isEmpty()) {
    label += tr(" in ") + in_label.value();
  }

  if (!label.isEmpty()) {
    emit LabelForItem(item_id, label);
  }
}

}  // namespace mx::gui
