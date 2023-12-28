/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "ExpandTreeExplorerThread.h"

#include <multiplier/GUI/ITreeGenerator.h>

namespace mx::gui {

void ExpandTreeExplorerThread::run(void) {
  QList<std::shared_ptr<ITreeItem>> items;
  for (auto item : d->generator->Children(d->generator, d->parent_entity_id)) {
    if (d->version_number.load() != d->captured_version_number) {
      return;
    }
    items.emplaceBack(std::move(item));
  }
  if (d->version_number.load() != d->captured_version_number) {
    return;
  }
  emit NewTreeItems(d->captured_version_number, d->parent_entity_id, items,
                    d->depth - 1u);
}

}  // namespace mx::gui
