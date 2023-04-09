/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/Util.h>

#include <QString>

#include <gap/core/generator.hpp>

#include <unordered_map>

namespace mx::gui {

// Return the ID of the entity with a name that contains `entity`.
VariantEntity NamedEntityContaining(const VariantEntity &entity,
                                    const VariantEntity &containing);

//! Generate references to the entity with `entity`. The references
//! pairs of named entities and the referenced entity. Sometimes the
//! referenced entity will match the named entity, other times the named
//! entity will contain the reference (e.g. a function containing a call).
gap::generator<std::pair<VariantEntity, VariantEntity>>
References(const VariantEntity &entity);

}  // namespace mx::gui
