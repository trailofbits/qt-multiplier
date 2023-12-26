/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <memory>
#include <QString>
#include <QVariant>
#include <QWidget>

namespace mx::gui {

class ActionRegistry;
class ISyncAction;
class IAsyncAction;
class RootAction;

class IAction {
 private:
  friend class ISyncAction;
  friend class IAsyncAction;

  IAction(void) = default;

 public:
  virtual ~IAction(void);

  // Globally unique verb name associated with this signal. Verb names should
  // be namespaced, e.g. `com.trailofbits.TopLevelActionName` or
  // `com.trailofbits.PluginName.ActionName`.
  virtual QString Verb(void) const noexcept = 0;

  // Trigger this action. Triggering an action is an asynchronous operation,
  // and schedules the action to run sometime in the future.
  void Trigger(const QVariant &input);

 protected:
  friend class ActionRegistry;
  friend class RootAction;

  // Apply the action.
  virtual void Run(const QVariant &input) noexcept = 0;
};

// A special type of action that always executes immediately when triggered.
class ISyncAction : public IAction {
 public:
  ISyncAction(void) = default;

  virtual ~ISyncAction(void);
};

// A special type of action that never executes immediately when triggered,
// and is "sent" to a worker thread to run. This allows the action to perform
// blocking operations.
class IAsyncAction : public IAction {
 public:
  IAsyncAction(void) = default;

  virtual ~IAsyncAction(void);
};

}  // namespace mx::gui

Q_DECLARE_INTERFACE(mx::gui::IAction, "com.trailofbits.IAction")
