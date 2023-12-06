/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/Index.h>
#include <multiplier/Types.h>

#include <QVariant>
#include <QWidget>
#include <QString>

#include <memory>
#include <functional>
#include <unordered_set>
#include <unordered_map>

namespace mx::gui {

class IActionRegistry {
 public:
  using Ptr = std::unique_ptr<IActionRegistry>;
  static Ptr Create(Index index, FileLocationCache file_location_cache);

  IActionRegistry() = default;
  virtual ~IActionRegistry() = default;

  struct Action final {
    //! \todo Add more types here!
    enum class InputType {
      String,
      Integer,
      EntityIdentifier,
    };

    QString name;
    QString verb;

    std::unordered_set<InputType> input_type_list;

    std::function<bool(Index index, const QVariant &input)> check_input;
    std::function<bool(Index index, const QVariant &input, QWidget *parent)>
        invoke;
  };

  virtual bool Register(const Action &action) = 0;
  virtual bool Unregister(const QString &verb) = 0;

  virtual std::unordered_map<QString, QString>
  GetCompatibleActions(const QVariant &input) const = 0;

  virtual bool Execute(const QString &verb, const QVariant &input,
                       QWidget *parent = nullptr) const = 0;

  IActionRegistry(const IActionRegistry &) = delete;
  IActionRegistry &operator=(const IActionRegistry &) = delete;
};

}  // namespace mx::gui
