// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/Entities/Attr.h>
#include <multiplier/Entities/CallExpr.h>
#include <multiplier/Entities/CXXBaseSpecifier.h>
#include <multiplier/Entities/DefineMacroDirective.h>
#include <multiplier/Entities/Designator.h>
#include <multiplier/Entities/File.h>
#include <multiplier/Entities/Fragment.h>
#include <multiplier/Entities/FunctionDecl.h>
#include <multiplier/Entities/IncludeLikeMacroDirective.h>
#include <multiplier/Entities/MacroExpansion.h>
#include <multiplier/Entities/Stmt.h>
#include <multiplier/Entities/TemplateArgument.h>
#include <multiplier/Entities/TemplateParameterList.h>
#include <multiplier/Entities/Token.h>
#include <multiplier/Entities/Type.h>
#include <multiplier/Entity.h>
#include <optional>
#include <QString>
#include <tuple>

namespace mx::gui {

struct EntityInformation final {
  struct Location {
    File file;
    unsigned line;
    unsigned column;

    inline Location(File file_, unsigned line_, unsigned column_)
        : file(std::move(file_)),
          line(line_),
          column(column_) {}
  };

  struct Selection {
    VariantEntity entity;
    TokenRange tokens;
    std::optional<Location> location;
  };

  //! The entity ID which was requested.
  RawEntityId requested_id{kInvalidEntityId};

  //! If `requested_id` is a declaration ID, then this is the canonical
  //! declaration ID. Otherwise, it is the same as `requested_id`.
  RawEntityId id{kInvalidEntityId};

  //! The entity associated with `id`.
  Selection entity;

  // The name of the requested thing.
  QString name;

  //! If `entity` is a decl with redeclarations, then this is the list of
  //! redeclarations. Each selection will reference a `NamedDecl`.
  std::vector<Selection> redeclarations;

  //! If `entity` is a declaration, then this is the set of macros used in the
  //! declaration (or its redeclarations), including the bod(ies) of any
  //! definitions. Each selection will reference a `MacroExpansion`.
  std::vector<Selection> macros_used;

  //! If `entity` is a function, then this is the list of functions that
  //! `entity` calls. Each selection will reference a `FunctionDecl`.
  std::vector<Selection> callees;

  //! If `entity` is a function, then this is the list of functions calling
  //! `entity`. Each selection will reference a `FunctionDecl`.
  std::vector<Selection> callers;

  //! If `entity` is a variable, then this is the list of variables to which
  //! `entity` is assigned. This could be a `VarDeclStmt`, a `BinaryOperator`,
  //! or a `DeclRefExpr` where the variable is passed as a paremter to another
  //! function. Each selection will reference a `Stmt`.
  std::vector<Selection> assigned_tos;

  //! If `entity` is a variable, then this is the list of assignments to
  //! `entity`. In the case of parameters, this will include arguments passed
  //! to the function. Each selection will reference a `Stmt`.
  std::vector<Selection> assignments;

  //! If `entity` is a file, then this is the set of `#include`-like directives
  //! inside of the file. Each selection will reference an
  //! `IncludeLikeMacroDirective`.
  std::vector<Selection> includes;

  //! If `entity` is a file, then this is the set of `#include`-like directives
  //! that reference this file. Each selection will reference an
  //! `IncludeLikeMacroDirective`.
  std::vector<Selection> include_bys;

  //! If `entity` is a file, then this is the set of top-level entities in this
  //! file. Each selection will reference a `DefineMacroDirective` or a `Decl`.
  std::vector<Selection> top_level_entities;
};

}  // namespace mx::gui
