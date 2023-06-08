/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/Types.h>
#include <multiplier/Token.h>

#include <QColor>

#include <unordered_map>
#include <unordered_set>

#include <optional>

namespace mx::gui {

using EntityColorMap = std::unordered_map<RawEntityId, QColor>;

struct EntityHighlight final {
  QString name;
  std::optional<Token> opt_name_token;
  RawEntityId primary_entity_id{};
  QColor color;
  std::unordered_set<RawEntityId> entity_id_list;
};

using EntityHighlightList = std::vector<EntityHighlight>;

}  // namespace mx::gui
