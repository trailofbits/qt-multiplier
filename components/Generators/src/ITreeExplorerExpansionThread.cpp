/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "ITreeExplorerExpansionThread.h"

#include <multiplier/GUI/ITreeGenerator.h>

namespace mx::gui {

ITreeExplorerExpansionThread::~ITreeExplorerExpansionThread(void) {}

ITreeExplorerExpansionThread::ITreeExplorerExpansionThread(
    std::shared_ptr<ITreeGenerator> generator_,
    const std::atomic_uint64_t &version_number, RawEntityId parent_entity_id,
    unsigned depth)
    : d(new ThreadData(std::move(generator_), version_number, parent_entity_id,
                       depth)) {
  setAutoDelete(true);
}

ITreeExplorerExpansionThread::ThreadData::~ThreadData(void) {}

}  // namespace mx::gui
