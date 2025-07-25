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
  QVector<IGeneratedItemPtr> items;
  for (auto item : generator->Roots(generator)) {
    if (version_number.load() != captured_version_number) {
      emit Finished();
      return;
    }

    items.emplaceBack(std::move(item));

    // Send out a batch.
    if (items.size() >= kMaxBatchSize) {
      emit NewGeneratedItems(captured_version_number, parent_item_id,
                             std::move(items), depth - 1u);
      items.clear();
    }
  }

  if (version_number.load() != captured_version_number) {
    emit Finished();
    return;
  }

  emit NewGeneratedItems(captured_version_number, parent_item_id,
                         std::move(items), depth - 1u);
  emit Finished();
}

}  // namespace mx::gui
