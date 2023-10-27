// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "CallHierarchyGenerator.h"

#include <filesystem>
#include <multiplier/Entity.h>
#include <multiplier/Token.h>
#include <multiplier/ui/Util.h>

Q_DECLARE_METATYPE(mx::TokenRange);

namespace mx::gui {
namespace {

class CallHierarchyItem final : public ITreeItem {

  RawEntityId entity_id;
  TokenRange name_tokens;
  QString location;
  QString breadcrumbs;

 public:
  virtual ~CallHierarchyItem(void) = default;

  inline CallHierarchyItem(RawEntityId entity_id_, TokenRange name_tokens_,
                           QString location_, QString breadcrumbs_)
      : entity_id(entity_id_),
        name_tokens(std::move(name_tokens_)),
        location(std::move(location_)),
        breadcrumbs(std::move(breadcrumbs_)) {}

  static std::shared_ptr<ITreeItem> Create(
      const FileLocationCache &file_location_cache,
      const VariantEntity &use, const VariantEntity &entity);

  // NOTE(pag): This must be non-blocking.
  RawEntityId EntityId(void) const Q_DECL_FINAL {
    return entity_id;
  }

  QVariant Data(int col) const Q_DECL_FINAL {
    QVariant data;
    switch (col) {
      case 0:
        data.setValue(name_tokens);
        break;
      case 1:
        data.setValue(location);
        break;
      case 2:
        data.setValue(breadcrumbs);
        break;
      default:
        break;
    }
    return data;
  }
};

static QString FilePath(const File &file) {
  for (auto path : file.paths()) {
    return QString::fromStdString(path.generic_string());
  }
  return QString();
}

static QString FileLineColumn(const File &file, unsigned line, unsigned col) {
  return QString("%1:%2:%3").arg(FilePath(file)).arg(line).arg(col);
}

static QString Location(const FileLocationCache &file_location_cache,
                        const VariantEntity &entity) {

  QString location;
  for (Token tok : FileTokens(entity)) {
    auto file = File::containing(tok);
    if (!file) {
      continue;
    }

    if (auto line_col = tok.location(file_location_cache)) {
      return FileLineColumn(file.value(), line_col->first, line_col->second);
    }

    location = FilePath(file.value());
  }

  return location;
}

static QString BreadCrumbs(const VariantEntity &entity) {
  if (auto bc = EntityBreadCrumbs(entity)) {
    return bc.value();
  }

  return QString();
}

// Given that `use` is a use of `entity`, get us information about it.
std::shared_ptr<ITreeItem> CallHierarchyItem::Create(
    const FileLocationCache &file_location_cache,
    const VariantEntity &use, const VariantEntity &entity) {

  return std::make_shared<CallHierarchyItem>(
      ::mx::EntityId(use).Pack(),
      NameOfEntity(entity),
      Location(file_location_cache, use),
      BreadCrumbs(use));
}

static VariantEntity NamedEntityContaining(const VariantEntity &entity) {
  if (std::holds_alternative<Decl>(entity)) {

    if (auto cd = NamedDeclContaining(std::get<Decl>(entity));
        !std::holds_alternative<NotAnEntity>(cd)) {
      return cd;
    }

    if (auto nd = NamedDecl::from(std::get<Decl>(entity))) {
      return nd->canonical_declaration();
    }

    // TODO(pag): Do token-based lookup?

  } else if (std::holds_alternative<Stmt>(entity)) {
    const Stmt &stmt = std::get<Stmt>(entity);

    if (auto nd = NamedDeclContaining(stmt);
        !std::holds_alternative<NotAnEntity>(nd)) {
      return nd;
    }

    // TODO(pag): Do token-based lookup?

    if (auto file = File::containing(stmt)) {
      return file.value();
    }

  } else if (std::holds_alternative<Macro>(entity)) {

    // It could be that we are looking at an expansion that isn't actually
    // used per se (e.g. the expansion happens as a result of PASTA eagerly
    // doing argument pre-expansions), but only the macro name gets used, so
    // we can't connect any final parsed tokens to anything, and thus we want
    // to instead go and find the root of the expansion and ask for the named
    // declaration containing that.
    //
    // Another reason to look at the root macro expansion is that we may be
    // asking for a use of a define that is in the same fragment as the
    // expansion, and we don't want the expansion to put us into the body of
    // a define, but to the use of the top-level macro expansion.
    Macro macro = std::move(std::get<Macro>(entity)).root();

    for (Token tok : macro.generate_expansion_tokens()) {
      if (Token pt = tok.parsed_token()) {
        if (auto nd = NamedDeclContaining(pt);
            !std::holds_alternative<NotAnEntity>(nd)) {
          return nd;
        }
      }
    }

    // If the macro wasn't used inside of a decl/statement, then go try to
    // find the macro definition containing this macro.
    if (auto dd = DefineMacroDirective::from(macro)) {
      return dd.value();
    }

  } else if (std::holds_alternative<File>(entity)) {
    return entity;

  } else if (std::holds_alternative<Fragment>(entity)) {
    if (auto file = File::containing(std::get<Fragment>(entity))) {
      return file.value();
    }

  } else if (std::holds_alternative<Designator>(entity)) {
    const Designator &d = std::get<Designator>(entity);
    if (auto fd = d.field()) {
      return fd.value();
    }

  } else if (std::holds_alternative<Token>(entity)) {
    const Token &tok = std::get<Token>(entity);

    if (auto pt = tok.parsed_token()) {
      if (auto nd = NamedDeclContaining(pt);
          !std::holds_alternative<NotAnEntity>(nd)) {
        return nd;
      }
    }

    for (Macro m : Macro::containing(tok)) {
      if (auto ne = NamedEntityContaining(std::move(m));
          !std::holds_alternative<NotAnEntity>(ne)) {
        return ne;
      }
    }

    if (auto dt = tok.derived_token()) {
      if (auto nd = NamedDeclContaining(dt);
          !std::holds_alternative<NotAnEntity>(nd)) {
        return nd;
      }
    }

    if (std::optional<Fragment> frag = Fragment::containing(tok)) {
      for (NamedDecl nd : NamedDecl::in(*frag)) {
        if (nd.tokens().index_of(tok)) {
          return nd;
        }
      }
    }
  }

  // TODO(pag): CXXBaseSpecifier, CXXTemplateArgument, CXXTemplateParameterList.

  return NotAnEntity{};
}

// Generate references to the entity with `entity`. The references
// pairs of named entities and the referenced entity. Sometimes the
// referenced entity will match the named entity, other times the named
// entity will contain the reference (e.g. a function containing a call).
gap::generator<> EnumerateEntityReferences(
    const VariantEntity &entity,
    const std::function<bool(const VariantEntity &, const VariantEntity &)>
        &callback) {

  if (std::holds_alternative<NotAnEntity>(entity)) {
    return;

#define REFERENCES_TO_CATEGORY(type_name, lower_name, enum_name, category) \
  } \
  else if (std::holds_alternative<type_name>(entity)) { \
    if (result_promise.isCanceled()) { \
      return; \
    } \
\
    for (Reference ref : std::get<type_name>(entity).references()) { \
      if (result_promise.isCanceled()) { \
        return; \
      } \
      VariantEntity rd = ref.as_variant(); \
      VariantEntity nd = NamedEntityContaining(rd, entity); \
      if (!std::holds_alternative<NotAnEntity>(nd)) { \
        if (!callback(nd, rd)) { \
          return; \
        } \
      } \
    }

    MX_FOR_EACH_ENTITY_CATEGORY(REFERENCES_TO_CATEGORY, REFERENCES_TO_CATEGORY,
                                REFERENCES_TO_CATEGORY,
                                MX_IGNORE_ENTITY_CATEGORY,
                                REFERENCES_TO_CATEGORY, REFERENCES_TO_CATEGORY,
                                MX_IGNORE_ENTITY_CATEGORY)

  } else {
    return;
  }
}

}  // namespace

int CallHierarchyGenerator::NumColumns(void) const {
  return 3;
}

QVariant CallHierarchyGenerator::ColumnTitle(int col) const {
  switch (col) {
    case 0: return tr("Entity");
    case 1: return tr("File name");
    case 2: return tr("Breadcrumbs");
    default: return QVariant();
  }
}

QString CallHierarchyGenerator::TreeName(
    const std::shared_ptr<ITreeGenerator> &self) const {

  if (auto name = NameOfEntityAsString(index.entity(root_entity_id))) {
    return tr("Call hierarchy of `%1`").arg(name.value());
  
  } else {
    return tr("Call hierarchy of entity %1").arg(root_entity_id);
  }
}

gap::generator<std::shared_ptr<ITreeItem>> CallHierarchyGenerator::Roots(
    const std::shared_ptr<ITreeGenerator> &self) {

  VariantEntity entity = index.entity(root_entity_id);
  if (std::holds_alternative<NotAnEntity>(entity)) {
    co_return;
  }

  if (std::holds_alternative<Decl>(entity)) {
    for (Decl redecl : std::get<Decl>(entity).redeclarations()) {
      co_yield CallHierarchyItem::Create(file_location_cache, redecl, redecl);
    }

  } else {
    co_yield CallHierarchyItem::Create(file_location_cache, entity, entity);
  }
}

gap::generator<std::shared_ptr<ITreeItem>> CallHierarchyGenerator::Children(
    const std::shared_ptr<ITreeGenerator> &self,
    RawEntityId parent_entity) {
  
  VariantEntity entity = index.entity(root_entity_id);
  if (std::holds_alternative<NotAnEntity>(entity)) {
    co_return;
  }

  VariantEntity containing_entity = NamedEntityContaining(entity);
  if (std::holds_alternative<NotAnEntity>(containing_entity)) {
    co_return;
  }

  for (Reference ref : Reference::from(containing_entity)) {
    auto use = ref.as_variant();
    auto user = NamedEntityContaining(use);
    co_yield CallHierarchyItem::Create(file_location_cache, use, user);
  }
}

}  // namespaxce mx::gui
