/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/IDatabase.h>

#include <multiplier/Index.h>

#include <QPromise>
#include <QString>

namespace mx::gui {

//! Returns the references for the given entity id
void GetEntityReferences(QPromise<bool> &result_promise, const Index &index,
                         const FileLocationCache &file_location_cache,
                         IDatabase::QueryEntityReferencesReceiver *receiver,
                         RawEntityId entity_id,
                         IDatabase::ReferenceType reference_type,
                         bool include_redeclarations, bool emit_root_node,
                         std::size_t depth);

}  // namespace mx::gui
