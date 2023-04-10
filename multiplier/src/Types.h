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
    RawEntityId entity_id{kInvalidEntityId};
    QString name;
  };

  using ItemList = std::list<Item>;

  ItemList item_list;

  // NOTE(pag): Normally, this points to `item_list.end()`, meaning that
  //            everything in `item_list` is "in our past." We don't have an
  //            active item that tracks our current location because that is
  //            actively maintained by a property of the currently active
  //            file code tab.
  ItemList::iterator current_item_it{};

  inline History(void)
      : current_item_it(item_list.end()) {}
};

}  // namespace mx::gui
