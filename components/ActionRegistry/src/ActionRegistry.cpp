/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "ActionRegistry.h"

#include <vector>

namespace mx::gui {

TriggerHandleImpl::~TriggerHandleImpl(void) {}

void TriggerHandle::Trigger(const QVariant &data) const noexcept {
  d->Trigger(data);
}

ActionRegistry::PrivateData::PrivateData(void) {}
ActionRegistry::PrivateData::~PrivateData(void) {}

TriggerHandleImpl *ActionRegistry::PrivateData::TriggerFor(const QString &verb) {
  std::unique_lock<std::mutex> locker(named_triggers_lock);

  auto trigger_it = named_triggers.find(verb);
  if (trigger_it == named_triggers.end()) {
    TriggerHandleImpl &trigger = owned_triggers.emplace_back(nullptr);
    trigger_it = named_triggers.insert(verb, &trigger);
  }

  return *trigger_it;
}

ActionRegistry::ActionRegistry(void)
    : d(std::make_shared<PrivateData>()) {}

ActionRegistry::~ActionRegistry(void) {}

// We always return the `RootAction`. 
TriggerHandle ActionRegistry::Find(const QString &verb) const {
  return std::shared_ptr<TriggerHandleImpl>(d, d->TriggerFor(verb));
}

// Register an action with the action registry.
TriggerHandle ActionRegistry::Register(IAction &action) {
  TriggerHandleImpl *trigger = d->TriggerFor(action.Verb());

  QObject::connect(trigger, &TriggerHandleImpl::Triggered,
                   &action, &IAction::Run);

  return std::shared_ptr<TriggerHandleImpl>(d, trigger);
}

}  // namespace mx::gui
