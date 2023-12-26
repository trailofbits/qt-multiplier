/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "ActionRegistry.h"

#include <QRunnable>

namespace mx::gui {
namespace {

class ActionRunner Q_DECL_FINAL : public QRunnable {
 public:
  const QVariant input;
  RootAction * const root_action;

  virtual ~ActionRunner(void) = default;

  inline ActionRunner(const QVariant &input_, RootAction *root_action_)
      : input(input_),
        root_action(root_action_) {}

  virtual void run(void) Q_DECL_FINAL {
    root_action->Run(input);
  }
};

}  // namespace

IAction::~IAction(void) {}
ISyncAction::~ISyncAction(void) {}
IAsyncAction::~IAsyncAction(void) {}

// Trigger this action. Triggering an action is an asynchronous operation,
// and schedules the action to run sometime in the future.
void IAction::Trigger(const QVariant &input) {
  auto root_action = dynamic_cast<RootAction *>(this);
  if (!root_action) {
    return;
  }

  std::unique_lock<std::mutex> locker(root_action->actions_lock);
  
  for (const auto &action_ptr : root_action->sync_actions) {
    if (auto action = action_ptr.load()) {
      action->Run(input);
    }
  }

  if (!root_action->async_actions.empty()) {
    auto runnable = new ActionRunner(input, root_action);
    runnable->setAutoDelete(true);
    root_action->runner.start(runnable);
  }
}

}  // namespace mx::gui
