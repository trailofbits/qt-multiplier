// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/AST/Attr.h>
#include <multiplier/AST/CallExpr.h>
#include <multiplier/AST/CXXBaseSpecifier.h>
#include <multiplier/AST/Designator.h>
#include <multiplier/AST/FunctionDecl.h>
#include <multiplier/AST/Stmt.h>
#include <multiplier/AST/TemplateArgument.h>
#include <multiplier/AST/TemplateParameterList.h>
#include <multiplier/AST/Type.h>
#include <multiplier/Entity.h>
#include <multiplier/Fragment.h>
#include <multiplier/Frontend/Compilation.h>
#include <multiplier/Frontend/DefineMacroDirective.h>
#include <multiplier/Frontend/File.h>
#include <multiplier/Frontend/IncludeLikeMacroDirective.h>
#include <multiplier/Frontend/MacroExpansion.h>
#include <multiplier/Frontend/Token.h>
#include <multiplier/IR/Operation.h>

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
