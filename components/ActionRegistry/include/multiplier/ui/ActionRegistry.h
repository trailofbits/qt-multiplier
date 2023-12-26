/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/Index.h>
#include <multiplier/Types.h>

#include <QVariant>
#include <QWidget>
#include <QString>

#include <atomic>
#include <memory>
#include <functional>
#include <unordered_set>
#include <unordered_map>

namespace mx::gui {

class ActionRegistry;
class IAction;

// A handle to a registered, or once-registered action. When an `IAction` is
// registered with an `ActionRegistry`, the `ActionRegistry` gives the
// registree the ability to unregister the action with this handle.
class ActionHandle {
 public:
  
  using IActionPtr = std::shared_ptr<std::atomic<IAction *>>;

 private:
  friend class ActionRegistry;

  struct PrivateData;

  IActionPtr action;

  inline ActionHandle(IActionPtr action_)
      : action(std::move(action_)) {}

 public:
  inline ~ActionHandle(void) {
    action->store(nullptr);
  }

  ActionHandle(void) = default;

  // Unregister an action. Returns `true` if the corresponding action was
  // unregistered, and `false` if it has already been disconnected.
  inline bool Unregister(void) {
    return action->exchange(nullptr) != nullptr;
  }
};

// Registry for actions.
class ActionRegistry {
 private:
  struct PrivateData;

  std::shared_ptr<PrivateData> d;

 public:
  ActionRegistry(void);
  ~ActionRegistry(void);

  // Look up an action by its name, and return an `IAction` that can be
  // triggered. This always returns a valid action.
  //
  // TOOD(pag): Consider returning some kind of `TriggerHandle`, that is a
  //            bit better/safer with the memory management.
  IAction &LookUp(const QString &verb) const;

  // Register an action with the action registry.
  ActionHandle Register(IAction &action);
};

}  // namespace mx::gui
