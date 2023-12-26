/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "ActionRegistry.h"

#include <vector>

namespace mx::gui {

RootAction::~RootAction(void) {}

// Globally unique verb name associated with this signal. 
QString RootAction::Verb(void) const noexcept {
  return verb;
}

// Apply the action. This never executes on the main GUI thread, so it's safe
// for it to do blocking operations.
void RootAction::Run(const QVariant &input) noexcept {
  std::unique_lock<std::mutex> locker(actions_lock);
  for (const auto &action_ptr : async_actions) {
    if (auto action = action_ptr.load()) {
      action->Run(input);
    }
  }
}

ActionRegistry::PrivateData::PrivateData(void) {}
ActionRegistry::PrivateData::~PrivateData(void) {}

RootAction &ActionRegistry::PrivateData::RootActionFor(const QString &verb) {
  std::unique_lock<std::mutex> locker(named_actions_lock);

  auto action_it = named_actions.find(verb);
  if (action_it == named_actions.end()) {
    auto action = new RootAction(verb, runner);
    owned_actions.emplace_back(action);
    action_it = named_actions.insert(verb, action);
  }

  return **action_it;
}

ActionRegistry::ActionRegistry(void)
    : d(std::make_shared<PrivateData>()) {}

ActionRegistry::~ActionRegistry(void) {}

// We always return the `RootAction`. 
IAction &ActionRegistry::LookUp(const QString &verb) const {
  return d->RootActionFor(verb);
}

// Register an action with the action registry.
ActionHandle ActionRegistry::Register(IAction &action) {
  if (dynamic_cast<RootAction *>(&action)) {
    return {};
  }

  auto &root_action = d->RootActionFor(action.Verb());

  std::atomic<IAction *> *action_ptr = nullptr;
  {
    std::unique_lock<std::mutex> locker(root_action.actions_lock);
    if (dynamic_cast<IAsyncAction *>(&action)) {
      action_ptr = &(root_action.async_actions.emplace_back(&action));

    } else if (dynamic_cast<ISyncAction *>(&action)) {
      action_ptr = &(root_action.sync_actions.emplace_back(&action));
    }
  }

  return ActionHandle(ActionHandle::IActionPtr(d, action_ptr));
}

}  // namespace mx::gui
