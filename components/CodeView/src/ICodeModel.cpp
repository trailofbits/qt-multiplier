/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/ui/ICodeView2.h>

#include "CodeModel.h"

namespace mx {

ICodeModel *ICodeModel::Create(const FileLocationCache &file_location_cache,
                               Index index, QObject *parent) {
  return new CodeModel(file_location_cache, index, parent);
}

}  // namespace mx
