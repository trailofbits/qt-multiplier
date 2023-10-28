/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <cstdint>
#include <gap/core/generator.hpp>
#include <memory>
#include <multiplier/Types.h>

#include <QObject>
#include <QString>
#include <QVariant>

namespace mx::gui {

//! A generated tree item from an `ITreeGenerator`.
class ITreeItem {
 public:
  virtual ~ITreeItem(void) = default;

  // The entity ID that uniquely identifies this tree item.
  //
  // NOTE(pag): This must be non-blocking.
  virtual RawEntityId EntityId(void) const = 0;

  // Returns the entity ID aliased by this entity, or `kInvalidEntityId`. This
  // is a means of communicating equivalence of rows in terms of their
  // child sets, but not necessarily in terms of their `Data`.
  //
  // NOTE(pag): If this returns a valid entity ID, then it must be one that was
  //            associated with an item generated prior to this `ITreeItem` in
  //            the current tree.
  virtual RawEntityId AliasedEntityId(void) const;

  // Column data for the tree item.
  //
  // NOTE(pag): This must be non-blocking.
  virtual QVariant Data(int) const = 0;
};

//! Data generator for an entity tree. The data generator can be arbitrarily
//! slow at generating its data.
class ITreeGenerator : public QObject {
  Q_OBJECT

 public:
  virtual ~ITreeGenerator(void) = default;

  // Return the number of columns of data.
  //
  // NOTE(pag): This must be non-blocking.
  virtual int NumColumns(void) const = 0;

  // Return the `Nth` column title.
  //
  // NOTE(pag): This must be non-blocking.
  virtual QVariant ColumnTitle(int) const = 0;

  // Return the name of this tree.
  //
  // NOTE(pag): This is allowed to block.
  virtual QString TreeName(
      const std::shared_ptr<ITreeGenerator> &self) const = 0;

  // Generate the root / top-level items for the tree. Defaults to
  // `Children(kInvalidEntityId)`.
  //
  // NOTE(pag): These are `shared_ptr`s so that implementations have the
  //            flexibility of having tree items extend the lifetime of
  //            tree generator (`self`) itself via aliasing `std::shared_ptr`.
  //
  // NOTE(pag): This is allowed to block.
  virtual gap::generator<std::shared_ptr<ITreeItem>> Roots(
      const std::shared_ptr<ITreeGenerator> &self);

  // Given a parent entity ID, goes and fetches the tree items for the children
  // of the tree.
  //
  // NOTE(pag): These are `shared_ptr`s so that implementations have the
  //            flexibility of having tree items extend the lifetime of
  //            tree generator (`self`) itself via aliasing `std::shared_ptr`.
  //
  // NOTE(pag): This is allowed to block.
  virtual gap::generator<std::shared_ptr<ITreeItem>> Children(
      const std::shared_ptr<ITreeGenerator> &self,
      RawEntityId parent_entity) = 0;
};

}  // namespace mx::gui
