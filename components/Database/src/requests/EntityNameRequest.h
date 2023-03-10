/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/Index.h>

#include <QPromise>
#include <QString>

#include <optional>

namespace mx::gui {

using OptionalName = std::optional<QString>;

void CreateEntityNameRequest(QPromise<OptionalName> &entity_name_promise,
                             const Index &index, const RawEntityId &entity_id);

}  // namespace mx::gui
