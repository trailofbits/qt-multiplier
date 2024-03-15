// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/Registry.h>

#include <QString>
#include <QHash>

namespace mx::gui {

struct KeyInformation final {
  Registry::Type type{Registry::Type::String};
  QString localized_name;
  QString description;
};

//using KeyMap = QHash<QString, QHash<QString, KeyInformation>>;

KeyMap GetRegistryKeyMap(const Registry &registry);

}  // namespace mx::gui
