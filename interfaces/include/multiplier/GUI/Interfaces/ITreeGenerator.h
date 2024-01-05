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

#include "IGeneratedItem.h"

namespace mx::gui {

class ITreeGenerator;

using ITreeGeneratorPtr = std::shared_ptr<ITreeGenerator>;

//! Data generator for an entity tree. The data generator can be arbitrarily
//! slow at generating its data.
class ITreeGenerator : public std::enable_shared_from_this<ITreeGenerator> {
 public:
  virtual ~ITreeGenerator(void) = default;

  // Return the initialize expansion depth (defaults to `2`).
  virtual unsigned InitialExpansionDepth(void) const;

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
  Name(const ITreeGeneratorPtr &self) const = 0;

  // Generate the root / top-level items for the tree.
  //
  // NOTE(pag): These are `shared_ptr`s so that implementations have the
  //            flexibility of having tree items extend the lifetime of
  //            tree generator (`self`) itself via aliasing `std::shared_ptr`.
  //
  // NOTE(pag): This is allowed to block.
  virtual gap::generator<IGeneratedItemPtr> Roots(
      ITreeGeneratorPtr self) = 0;

  // Given a parent entity, goes and fetches the tree items for the children
  // of the tree.
  //
  // NOTE(pag): These are `shared_ptr`s so that implementations have the
  //            flexibility of having tree items extend the lifetime of
  //            tree generator (`self`) itself via aliasing `std::shared_ptr`.
  //
  // NOTE(pag): This is allowed to block.
  virtual gap::generator<IGeneratedItemPtr> Children(
      ITreeGeneratorPtr self, IGeneratedItemPtr parent_item) = 0;
};

}  // namespace mx::gui

Q_DECLARE_INTERFACE(mx::gui::ITreeGenerator,
                    "com.trailofbits.interface.ITreeGenerator")
