/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/ActionRegistry.h>

#include <atomic>
#include <deque>
#include <mutex>
#include <vector>

#include <QMap>

namespace mx::gui {

// All actions have a root action. The root actions are responsible to executing
// other actions. 
class TriggerHandleImpl final : public QObject {
  Q_OBJECT

 public:
  virtual ~TriggerHandleImpl(void);

  inline explicit TriggerHandleImpl(QObject *parent)
      : QObject(parent) {}

  inline void Trigger(const QVariant &input) {
    emit Triggered(input);
  }

 signals:
  void Triggered(const QVariant &input);
};

struct ActionRegistry::PrivateData {
 public:

  PrivateData(void);
  ~PrivateData(void);

  std::deque<TriggerHandleImpl> owned_triggers;
  QMap<QString, TriggerHandleImpl *> named_triggers;
  std::mutex named_triggers_lock;

  TriggerHandleImpl *TriggerFor(const QString &verb);
};

}  // namespace mx::gui
