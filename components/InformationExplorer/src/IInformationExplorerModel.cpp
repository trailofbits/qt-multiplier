/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "InformationExplorerModel.h"

namespace mx::gui {

IInformationExplorerModel *IInformationExplorerModel::Create(
    Index index, FileLocationCache file_location_cache, QObject *parent) {
  return new InformationExplorerModel(index, file_location_cache, parent);
}

}  // namespace mx::gui
