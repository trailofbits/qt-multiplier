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
class TriggerHandleImpl;

// Generic action. These actions are run on the main thread, so they should
// not arbitrarily block.
class IAction : public QObject {
  Q_OBJECT

 public:
  using QObject::QObject;

  virtual ~IAction(void);

  // Globally unique verb name associated with this signal. Verb names should
  // be namespaced, e.g. `com.trailofbits.TopLevelActionName` or
  // `com.trailofbits.PluginName.ActionName`.
  virtual QString Verb(void) const noexcept = 0;

 protected:
  friend class ActionRegistry;

 protected slots:
  virtual void Run(const QVariant &input) noexcept = 0;
};

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

  template <typename Lambda>
  TriggerHandle Register(QObject *parent, const QString &verb, Lambda lambda) {
    auto action = new LambdaAction(verb, std::move(lambda), parent);
    return Register(*action);
  }
};

}  // namespace mx::gui

Q_DECLARE_INTERFACE(mx::gui::IAction, "com.trailofbits.IAction")
