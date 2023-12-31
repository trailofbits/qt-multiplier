/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QString>

#include <gap/core/generator.hpp>
#include <memory>

#include "ITreeItem.h"

namespace mx::gui {

class ITreeGenerator;

using ITreeGeneratorPtr = std::shared_ptr<ITreeGenerator>;

//! Data generator for an entity tree. The data generator can be arbitrarily
//! slow at generating its data.
class ITreeGenerator : public std::enable_shared_from_this<ITreeGenerator> {
 public:
  virtual ~ITreeGenerator(void) = default;

  // Return the number of columns of data.
  //
  // NOTE(pag): This must be non-blocking.
  virtual int NumColumns(void) const = 0;

  // Return the `Nth` column title.
  //
  // NOTE(pag): This must be non-blocking.
  virtual QString ColumnTitle(int) const = 0;

  // Return the name of this tree.
  //
  // NOTE(pag): This is allowed to block.
  virtual QString
  TreeName(const ITreeGeneratorPtr &self) const = 0;

  // Generate the root / top-level items for the tree. Defaults to
  // `Children(self, NotAnEntity{})`.
  //
  // NOTE(pag): These are `shared_ptr`s so that implementations have the
  //            flexibility of having tree items extend the lifetime of
  //            tree generator (`self`) itself via aliasing `std::shared_ptr`.
  //
  // NOTE(pag): This is allowed to block.
  virtual gap::generator<ITreeItemPtr> Roots(const ITreeGeneratorPtr &self);

  // Given a parent entity ID, goes and fetches the tree items for the children
  // of the tree.
  //
  // NOTE(pag): These are `shared_ptr`s so that implementations have the
  //            flexibility of having tree items extend the lifetime of
  //            tree generator (`self`) itself via aliasing `std::shared_ptr`.
  //
  // NOTE(pag): This is allowed to block.
  virtual gap::generator<ITreeItemPtr> Children(
      const ITreeGeneratorPtr &self, const VariantEntity &parent_entity) = 0;
};

}  // namespace mx::gui

Q_DECLARE_INTERFACE(mx::gui::ITreeGenerator,
                    "com.trailofbits.interface.ITreeGenerator")
