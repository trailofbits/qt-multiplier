/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QObject>
#include <QString>
#include <QVariant>

namespace mx::gui {

class ActionManager;

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
  friend class ActionManager;

 protected slots:
  virtual void Run(const QVariant &input) noexcept = 0;
};

}  // namespace mx::gui

Q_DECLARE_INTERFACE(mx::gui::IAction, "com.trailofbits.interface.IAction")
