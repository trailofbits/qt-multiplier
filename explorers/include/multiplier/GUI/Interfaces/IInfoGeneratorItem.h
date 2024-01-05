/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/Entity.h>
#include <multiplier/Frontend/Token.h>

#include <QString>
#include <QVariant>

namespace mx::gui {

class IInfoGeneratorItem;

using IInfoGeneratorItemPtr = std::shared_ptr<const IInfoGeneratorItem>;

//! A generated tree item from an `ITreeGenerator`.
class IInfoGeneratorItem {
 public:
  virtual ~IInfoGeneratorItem(void);

  // The entity that uniquely identifies this item.
  //
  // NOTE(pag): This must be non-blocking.
  virtual VariantEntity Entity(void) const = 0;

  // The entity that uniquely identifies this tree item.
  //
  // NOTE(pag): This must be non-blocking.
  virtual TokenRange Tokens(void) const = 0;

  // The location string for this item. This is displayed in the case of
  // duplicated.
  //
  // NOTE(pag): This must be non-blocking.
  virtual QString Location(void) const = 0;
};

}  // namespace mx::gui

Q_DECLARE_INTERFACE(mx::gui::IInfoGeneratorItem,
                    "com.trailofbits.interface.IInfoGeneratorItem")
