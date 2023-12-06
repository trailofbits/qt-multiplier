/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/IActionRegistry.h>

namespace mx::gui {

class ActionRegistry final : public IActionRegistry {
 public:
  ActionRegistry(Index index, FileLocationCache file_location_cache);
  virtual ~ActionRegistry() override;

  virtual bool Register(const Action &action) override;
  virtual bool Unregister(const QString &verb) override;

  virtual std::unordered_map<QString, QString>
  GetCompatibleActions(const QVariant &input) const override;

  virtual bool Execute(const QString &verb, const QVariant &input,
                       QWidget *parent) const override;

  ActionRegistry(const ActionRegistry &) = delete;
  ActionRegistry &operator=(const ActionRegistry &) = delete;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;
};

}  // namespace mx::gui
