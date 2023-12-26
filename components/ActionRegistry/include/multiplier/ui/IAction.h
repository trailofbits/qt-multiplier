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

class IAction : public QObject {
  Q_OBJECT

  using QObject::QObject;

 public:
  IAction(void)
      : QObject(nullptr) {}

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

}  // namespace mx::gui

Q_DECLARE_INTERFACE(mx::gui::IAction, "com.trailofbits.IAction")
