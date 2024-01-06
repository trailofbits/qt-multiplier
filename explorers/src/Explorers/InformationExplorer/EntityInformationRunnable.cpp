// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "EntityInformationRunnable.h"

namespace mx::gui {

EntityInformationRunnable::~EntityInformationRunnable(void) {}

void EntityInformationRunnable::run(void) {
  QString category = generator->Category();
  QVector<IInfoGeneratorItemPtr> items;

  for (auto item : generator->Items(generator, file_location_cache)) {
    if (version_number->load() != captured_version_number) {
      emit Finished();
      return;
    }

    // TODO(pag): Add batching.
    items.emplaceBack(std::move(item));

    if (static_cast<size_t>(items.size()) >= kMaxBatchSize) {
      emit NewGeneratedItems(captured_version_number, category,
                             std::move(items));
    }
  }

  if (version_number->load() == captured_version_number) {
    emit NewGeneratedItems(captured_version_number, category, std::move(items));
  }

  emit Finished();
}

}  // namespace mx::gui
