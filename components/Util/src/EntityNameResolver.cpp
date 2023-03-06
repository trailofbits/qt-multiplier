// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "EntityNameResolver.h"

#include <multiplier/ui/Util.h>

#include <QThread>

namespace mx::gui {

struct EntityNameResolver::PrivateData final {
  Index index;
  RawEntityId entity_id{};
};

EntityNameResolver::EntityNameResolver(Index index,
                                       const RawEntityId &entity_id)
    : d(new PrivateData) {

  d->index = index;
  d->entity_id = entity_id;
}

EntityNameResolver::~EntityNameResolver() {}

void EntityNameResolver::Start() {
  auto &current_thread = *QThread::currentThread();

  std::unordered_map<RawEntityId, QString> file_paths;
  std::size_t processed_file_count{0};

  for (auto [path, id] : d->index.file_paths()) {
    if ((processed_file_count % 100) == 0) {
      if (current_thread.isInterruptionRequested()) {
        emit Finished(std::nullopt);
      }
    }

    file_paths.emplace(
        id.Pack(), QString::fromStdString(path.filename().generic_string()));
  }

  auto opt_name = NameOfEntity(d->index.entity(d->entity_id), file_paths);
  emit Finished(opt_name);
}

}  // namespace mx::gui
