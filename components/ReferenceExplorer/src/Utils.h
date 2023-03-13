/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/Entity.h>
#include <multiplier/Entities/Attr.h>
#include <multiplier/Entities/CXXBaseSpecifier.h>
#include <multiplier/Entities/DefineMacroDirective.h>
#include <multiplier/Entities/Designator.h>
#include <multiplier/Entities/FieldDecl.h>
#include <multiplier/Entities/File.h>
#include <multiplier/Entities/Fragment.h>
#include <multiplier/Entities/FunctionDecl.h>
#include <multiplier/Entities/IncludeLikeMacroDirective.h>
#include <multiplier/Entities/Stmt.h>
#include <multiplier/Entities/TemplateArgument.h>
#include <multiplier/Entities/TemplateParameterList.h>
#include <multiplier/Entities/Type.h>
#include <multiplier/Entities/TypeDecl.h>
#include <multiplier/Entities/VarDecl.h>
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

template <typename T>
static VariantEntity NamedDeclContaining(const T &thing) {
  for (FunctionDecl func : FunctionDecl::containing(thing)) {
    return func;
  }

  for (FieldDecl field : FieldDecl::containing(thing)) {
    return field;
  }

  for (VarDecl var : VarDecl::containing(thing)) {
    if (var.is_local_variable_declaration_or_parm()) {
      if (VariantEntity ent = NamedDeclContaining<Decl>(var);
          !std::holds_alternative<NotAnEntity>(ent)) {
        return ent;
      }

    } else {
      return var;
    }
  }

  for (NamedDecl nd : NamedDecl::containing(thing)) {
    return nd;
  }

  return NotAnEntity{};
}

}  // namespace mx::gui
