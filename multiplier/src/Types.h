// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/Index.h>

#include <QString>

#include <optional>
#include <list>

namespace mx::gui {

struct History final {
  struct Item final {
    RawEntityId file_id{};
    std::optional<RawEntityId> opt_entity_id{};
    QString name;
  };

  using ItemList = std::list<Item>;

  ItemList item_list;
  ItemList::iterator current_item_it{};

  History() {
    current_item_it = item_list.end();
  }
};

}  // namespace mx::gui
