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

#include <QString>
#include <QVariant>

#include <optional>
#include <tuple>

namespace mx::gui {

struct EntityLocation {
  File file;
  unsigned line;
  unsigned column;

  inline EntityLocation(File file_, unsigned line_, unsigned column_)
      : file(std::move(file_)),
        line(line_),
        column(column_) {}
};

struct EntityInformation {
  //! Category of this selection. E.g. `Definitions`, `Declarations`, etc.
  QString category;

  //! Used when computing the entity id for where clicking on this selection
  //! should take us.
  VariantEntity entity_role;

  //! This value is used to present the file:line:col of this selection. This
  //! might be different than where `entity_role` takes us. This can instead
  //! represent the "usage location." This is primarily used for
  //! deduplication.
  std::optional<EntityLocation> location;

  //! What should be displayed for this selection. This can be a `TokenRange`,
  //! a `Token`, or a `QString`.
  QVariant display_role;
};

}  // namespace mx::gui
