/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/Entity.h>

#include <QVariant>

namespace mx::gui {

class IGeneratedItem;

using IGeneratedItemPtr = std::shared_ptr<const IGeneratedItem>;

//! A generated tree item from an `ITreeGenerator`.
class IGeneratedItem {
 public:
  virtual ~IGeneratedItem(void);

  // The entity that uniquely identifies this tree item.
  //
  // NOTE(pag): This must be non-blocking.
  virtual VariantEntity Entity(void) const = 0;

  // Returns the entity aliased by this entity, or `NotAnEntity{}`. This
  // is a means of communicating equivalence of rows in terms of their
  // child sets, but not necessarily in terms of their `Data`.
  //
  // NOTE(pag): If this returns a valid entity ID, then it must be one that was
  //            associated with an item generated prior to this `IGeneratedItem` in
  //            the current tree.
  virtual RawEntityId AliasedEntityId(void) const;

  // Column data for the tree item.
  //
  // NOTE(pag): This must be non-blocking.
  virtual QVariant Data(int) const = 0;
};

}  // namespace mx::gui

Q_DECLARE_INTERFACE(mx::gui::IGeneratedItem,
                    "com.trailofbits.interface.IGeneratedItem")
