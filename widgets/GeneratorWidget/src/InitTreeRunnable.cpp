/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "InitTreeRunnable.h"

#include <multiplier/GUI/Interfaces/ITreeGenerator.h>

namespace mx::gui {

void InitTreeRunnable::run(void) {
  QList<IGeneratedItemPtr> items;
  for (auto item : generator->Roots(generator)) {
    if (version_number.load() != captured_version_number) {
      return;
    }
    items.emplaceBack(std::move(item));
  }
  if (version_number.load() != captured_version_number) {
    return;
  }
  emit NewGeneratedItems(captured_version_number, kInvalidEntityId, items,
                    depth - 1u);
}

}  // namespace mx::gui
