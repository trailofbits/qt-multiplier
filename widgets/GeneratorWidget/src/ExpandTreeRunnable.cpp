/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "ExpandTreeRunnable.h"

#include <multiplier/GUI/Interfaces/ITreeGenerator.h>

namespace mx::gui {

void ExpandTreeRunnable::run(void) {
  QList<IGeneratedItemPtr> items;
  for (auto item : generator->Children(generator, parent_entity)) {
    if (version_number.load() != captured_version_number) {
      emit Finished();
      return;
    }

    // TODO(pag): Add batching.
    items.emplaceBack(std::move(item));
  }

  if (version_number.load() != captured_version_number) {
    emit Finished();
    return;
  }

  emit NewGeneratedItems(
      captured_version_number, EntityId(parent_entity).Pack(),
      items, depth - 1u);
  emit Finished();
}

}  // namespace mx::gui
