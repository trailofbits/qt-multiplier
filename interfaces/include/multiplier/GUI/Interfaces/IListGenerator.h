/*
  Copyright (c) 2024-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include "ITreeGenerator.h"

namespace mx::gui {

class IListGenerator;

using IListGeneratorPtr = std::shared_ptr<IListGenerator>;

//! Data generator for an entity tree. The data generator can be arbitrarily
//! slow at generating its data.
class IListGenerator : public ITreeGenerator {
 public:
  virtual ~IListGenerator(void) = default;

  // Return the number of columns of data. This is always 1.
  int NumColumns(void) const Q_DECL_FINAL;

  // Return the initialize expansion depth (defaults to `1`).
  unsigned InitialExpansionDepth(void) const Q_DECL_FINAL;

  gap::generator<IGeneratedItemPtr> Children(
      const ITreeGeneratorPtr &, const VariantEntity &) Q_DECL_FINAL;
};

}  // namespace mx::gui

Q_DECLARE_INTERFACE(mx::gui::IListGenerator,
                    "com.trailofbits.interface.IListGenerator")
