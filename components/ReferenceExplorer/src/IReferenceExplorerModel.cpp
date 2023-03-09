/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "ReferenceExplorerModel.h"

#include <multiplier/ui/IReferenceExplorerModel.h>

namespace mx::gui {

IReferenceExplorerModel *
IReferenceExplorerModel::Create(mx::Index index,
                                mx::FileLocationCache file_location_cache,
                                QObject *parent) {
  return new ReferenceExplorerModel(index, file_location_cache, parent);
}

QDataStream &operator<<(QDataStream &stream,
                        const IReferenceExplorerModel::Location &location) {
  stream << location.path;
  stream << location.file_id;
  stream << location.line;
  stream << location.column;
  return stream;
}

QDataStream &operator>>(QDataStream &stream,
                        IReferenceExplorerModel::Location &location) {

  stream >> location.path;
  stream >> location.file_id;
  stream >> location.line;
  stream >> location.column;
  return stream;
}

}  // namespace mx::gui
