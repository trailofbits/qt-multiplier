// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "CallHierarchyGenerator.h"

#include <QDebug>

#include <filesystem>
#include <multiplier/Entity.h>
#include <multiplier/Token.h>
#include <multiplier/ui/Util.h>

Q_DECLARE_METATYPE(mx::TokenRange);

namespace mx::gui {
namespace {

class CallHierarchyItem final : public ITreeItem {

  RawEntityId entity_id;
  RawEntityId aliased_entity_id;
  TokenRange name_tokens;
  QString location;
  QString breadcrumbs;

 public:
  virtual ~CallHierarchyItem(void) = default;

  inline CallHierarchyItem(RawEntityId entity_id_,
                           RawEntityId aliased_entity_id_,
                           TokenRange name_tokens_,
                           QString location_,
                           QString breadcrumbs_)
      : entity_id(entity_id_),
        aliased_entity_id(aliased_entity_id_),
        name_tokens(std::move(name_tokens_)),
        location(std::move(location_)),
        breadcrumbs(std::move(breadcrumbs_)) {}

  static std::shared_ptr<ITreeItem> Create(
      const FileLocationCache &file_location_cache,
      const VariantEntity &use, const VariantEntity &entity,
      RawEntityId aliased_entity_id_=kInvalidEntityId);

  // NOTE(pag): This must be non-blocking.
  RawEntityId EntityId(void) const Q_DECL_FINAL {
    return entity_id;
  }

  // NOTE(pag): This must be non-blocking.
  RawEntityId AliasedEntityId(void) const Q_DECL_FINAL {
    return aliased_entity_id;
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

// Given that `use` is a use of `entity`, get us information about it.
std::shared_ptr<ITreeItem> CallHierarchyItem::Create(
    const FileLocationCache &file_location_cache,
    const VariantEntity &use, const VariantEntity &entity,
    RawEntityId aliased_entity_id_) {

  return std::make_shared<CallHierarchyItem>(
      ::mx::EntityId(use).Pack(),
      aliased_entity_id_,
      NameOfEntity(entity),
      LocationOfEntity(file_location_cache, use),
      EntityBreadCrumbs(use));
}

}  // namespace

int CallHierarchyGenerator::NumColumns(void) const {
  return 3;
}

QString CallHierarchyGenerator::ColumnTitle(int col) const {
  switch (col) {
    case 0: return tr("Entity");
    case 1: return tr("File name");
    case 2: return tr("Breadcrumbs");
    default: return QString();
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
    RawEntityId prev_redecl_id = kInvalidEntityId;
    for (Decl redecl : std::get<Decl>(entity).redeclarations()) {
      auto item = CallHierarchyItem::Create(file_location_cache, redecl, redecl,
                                            prev_redecl_id);
      prev_redecl_id = item->EntityId();
      co_yield item;
    }

  } else {
    co_yield CallHierarchyItem::Create(file_location_cache, entity, entity);
  }
}

gap::generator<std::shared_ptr<ITreeItem>> CallHierarchyGenerator::Children(
    const std::shared_ptr<ITreeGenerator> &self,
    RawEntityId parent_entity_id) {
  
  VariantEntity entity = index.entity(parent_entity_id);
  if (std::holds_alternative<NotAnEntity>(entity)) {
    co_return;
  }

  VariantEntity containing_entity = NamedEntityContaining(entity);
  if (std::holds_alternative<NotAnEntity>(containing_entity)) {
    co_return;
  }

  for (Reference ref : Reference::to(containing_entity)) {
    auto use = ref.as_variant();
    auto user = NamedEntityContaining(use);

    // We might have many uses of a thing, e.g. multiple calls to a function
    // A within a function B, and so we want the Nth call to reference the
    // first call.
    auto aliased_entity_id = kInvalidEntityId;
    if (std::holds_alternative<Decl>(user)) {
      aliased_entity_id =
          std::get<Decl>(user).canonical_declaration().id().Pack();
    }

    co_yield CallHierarchyItem::Create(
        file_location_cache, use, user, aliased_entity_id);
  }
}

}  // namespaxce mx::gui
