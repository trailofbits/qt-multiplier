/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "ActionRegistry.h"

namespace mx::gui {

struct ActionRegistry::PrivateData final {
  Index index;
  FileLocationCache file_location_cache;
  QMap<QString, Action> registered_action_map;
};

ActionRegistry::ActionRegistry(Index index,
                               FileLocationCache file_location_cache)
    : d(new PrivateData) {

  d->index = index;
  d->file_location_cache = file_location_cache;
}

ActionRegistry::~ActionRegistry() {}

bool ActionRegistry::Register(const Action &action) {
  if (d->registered_action_map.contains(action.verb)) {
    return false;
  }

  d->registered_action_map.insert(action.verb, action);
  return true;
}

bool ActionRegistry::Unregister(const QString &verb) {
  if (!d->registered_action_map.contains(verb)) {
    return false;
  }

  d->registered_action_map.remove(verb);
  return true;
}

std::unordered_map<QString, QString>
ActionRegistry::GetCompatibleActions(const QVariant &input) const {
  std::unordered_map<QString, QString> action_list;

  for (const auto &action : d->registered_action_map) {
    if (action.check_input(d->index, input)) {
      action_list.insert({action.name, action.verb});
    }
  }

  return action_list;
}

bool ActionRegistry::Execute(const QString &verb, const QVariant &input,
                             QWidget *parent) const {
  if (!d->registered_action_map.contains(verb)) {
    return false;
  }

  const auto &action = d->registered_action_map[verb];
  return action.invoke(d->index, input, parent);
}

}  // namespace mx::gui
