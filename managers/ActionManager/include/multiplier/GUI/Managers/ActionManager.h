/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <memory>
#include <multiplier/GUI/Interfaces/IAction.h>

namespace mx::gui {

class ActionManagerImpl;
class TriggerHandleImpl;

// An action that can wrap over a lambda.
template <typename Lambda>
class LambdaAction Q_DECL_FINAL : public IAction {
  const QString verb;
  Lambda callable;

 public:
  virtual ~LambdaAction(void) = default;

  inline LambdaAction(const QString &verb_, Lambda callable_, QObject *parent)
      : IAction(parent),
        verb(verb_),
        callable(std::move(callable_)) {}

  QString Verb(void) const noexcept Q_DECL_FINAL {
    return verb;
  }

  void Run(const QVariant &input) noexcept Q_DECL_FINAL {
    callable(input);
  }
};

// An action that can wrap over a lambda.
template <typename Class>
class MethodPointerAction Q_DECL_FINAL : public IAction {
  const QString verb;
  void (Class::*method)(const QVariant &);

 public:
  virtual ~MethodPointerAction(void) = default;

  inline MethodPointerAction(const QString &verb_,
                             void (Class::*method_)(const QVariant &),
                             Class *parent)
      : IAction(parent),
        verb(verb_),
        method(method_) {}

  QString Verb(void) const noexcept Q_DECL_FINAL {
    return verb;
  }

  void Run(const QVariant &input) noexcept Q_DECL_FINAL {
    (dynamic_cast<Class *>(parent())->*method)(input);
  }
};

// A handle on a registered action.
class TriggerHandle {
  friend class ActionManager;

  std::weak_ptr<TriggerHandleImpl> d;

  inline TriggerHandle(std::shared_ptr<TriggerHandleImpl> d_)
      : d(std::move(d_)) {}

 public:
  TriggerHandle(void) = default;
  ~TriggerHandle(void);

  operator bool(void) const noexcept;

  // Triggers an action.
  void Trigger(const QVariant &data) const noexcept;
};

struct NamedAction {
  QString name;
  TriggerHandle action;
  QVariant data;
};

// Registry for actions.
class ActionManager {
 private:
  std::shared_ptr<ActionManagerImpl> d;

 public:
  ActionManager(void);
  ~ActionManager(void);

  // Look up an action by its name, and return an `IAction` that can be
  // triggered. This always returns a valid action.
  TriggerHandle Find(const QString &verb) const;

  // Register an action with the action registry.
  TriggerHandle Register(IAction &action);

  template <typename Class>
  TriggerHandle Register(Class *parent, const QString &verb,
                         void (Class::*method)(const QVariant &)) {
    auto action = new MethodPointerAction<Class>(verb, method, parent);
    return Register(*action);
  }

  template <typename Lambda>
  TriggerHandle Register(QObject *parent, const QString &verb, Lambda lambda) {
    auto action = new LambdaAction<Lambda>(verb, std::move(lambda), parent);
    return Register(*action);
  }
};

}  // namespace mx::gui
