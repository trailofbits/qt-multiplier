// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "RegistryInternals.h"

#include <QHash>
#include <QString>

namespace mx::gui {

KeyMap GetRegistryKeyMap(const Registry &registry) {
  auto module_map = registry.ModuleMap();

  KeyMap key_map;
  for (auto it{module_map.begin()}; it != module_map.end(); ++it) {
    const auto &module_name = it.key();
    const auto &key_desc_map = it.value();

    QHash<QString, KeyInformation> local_key_map;
    for (const auto &key_desc : key_desc_map) {
      local_key_map.insert(
          key_desc.key_name,
          {key_desc.type, key_desc.localized_key_name, key_desc.description});
    }

    key_map.insert(module_name, std::move(local_key_map));
  }

  return key_map;
}

}  // namespace mx::gui
