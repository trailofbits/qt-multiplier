/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/ActionRegistry.h>
#include <multiplier/ui/IAction.h>

#include <atomic>
#include <deque>
#include <mutex>
#include <vector>

#include <QMap>
#include <QThreadPool>

namespace mx::gui {

// All actions have a root action. The root actions are responsible to executing
// other actions. 
class RootAction final : public IAction {
 public:
  const QString verb;
  
  // The list of actions for this root action.
  std::deque<std::atomic<IAction *>> sync_actions;
  std::deque<std::atomic<IAction *>> async_actions;

  // Lock on the actions.
  std::mutex actions_lock;

  QThreadPool &runner;

  virtual ~RootAction(void);

  inline RootAction(const QString &verb_, QThreadPool &runner_)
      : verb(verb_),
        runner(runner_) {}

  // Globally unique verb name associated with this signal. 
  QString Verb(void) const noexcept Q_DECL_FINAL;

  // Apply the action. This never executes on the main GUI thread, so it's safe
  // for it to do blocking operations with `index`.
  void Run(const QVariant &input) noexcept Q_DECL_FINAL;
};

struct ActionHandle::PrivateData {
  std::shared_ptr<IAction> action;

  inline PrivateData(std::shared_ptr<IAction> action_)
      : action(std::move(action_)) {}
};

struct ActionRegistry::PrivateData {
 public:

  PrivateData(void);
  ~PrivateData(void);

  std::vector<std::unique_ptr<RootAction>> owned_actions;
  QMap<QString, RootAction *> named_actions;
  std::mutex named_actions_lock;
  QThreadPool runner;

  RootAction &RootActionFor(const QString &verb);
};

}  // namespace mx::gui
