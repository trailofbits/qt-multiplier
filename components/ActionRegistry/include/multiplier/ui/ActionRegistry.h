/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QVariant>
#include <QString>

#include <memory>

namespace mx::gui {

class ActionRegistry;
class IAction;
class TriggerHandleImpl;

// A handle on a registered action.
class TriggerHandle {
  friend class ActionRegistry;

  std::shared_ptr<TriggerHandleImpl> d;

  inline TriggerHandle(std::shared_ptr<TriggerHandleImpl> d_)
      : d(std::move(d_)) {}

 public:

  // Triggers an action.
  void Trigger(const QVariant &data) const noexcept;
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
  TriggerHandle Find(const QString &verb) const;

  // Register an action with the action registry.
  TriggerHandle Register(IAction &action);
};

}  // namespace mx::gui
