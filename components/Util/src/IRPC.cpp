// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "RPC.h"

#include <multiplier/ui/IRPC.h>

namespace mx::gui {

IRPC::Ptr IRPC::Create(const Index &index,
                       const FileLocationCache &file_location_cache) {
  return Ptr(new RPC(index, file_location_cache));
}

}  // namespace mx::gui
