// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "CallHierarchyPlugin.h"

#include <multiplier/GUI/Managers/ActionManager.h>
#include <multiplier/GUI/Context.h>
#include <multiplier/GUI/IModel.h>

#include "CallHierarchyGenerator.h"

Q_DECLARE_METATYPE(mx::TokenRange);

namespace mx::gui {
namespace {

static const QKeySequence kKeySeqX("X");

static QString ActionName(const VariantEntity &) {
  return QObject::tr("Show Call Hierarchy");
}

class CallHierarchyItem final : public IGeneratedItem {

  VariantEntity entity;
  RawEntityId aliased_entity_id;
  TokenRange name_tokens;
  QString location;
  QString breadcrumbs;

 public:
  virtual ~CallHierarchyItem(void) = default;

  inline CallHierarchyItem(VariantEntity entity_,
                           RawEntityId aliased_entity_id_,
                           TokenRange name_tokens_, QString location_,
                           QString breadcrumbs_)
      : entity(std::move(entity_)),
        aliased_entity_id(aliased_entity_id_),
        name_tokens(std::move(name_tokens_)),
        location(std::move(location_)),
        breadcrumbs(std::move(breadcrumbs_)) {}

  VariantEntity EntityId(void) const Q_DECL_FINAL {
    return entity;
  }

  RawEntityId AliasedEntityId(void) const Q_DECL_FINAL {
    return aliased_entity_id;
  }

  QVariant Data(int col) const Q_DECL_FINAL {
    QVariant data;
    switch (col) {
      case 0: data.setValue(name_tokens); break;
      case 1: data.setValue(location); break;
      case 2: data.setValue(breadcrumbs); break;
      default: break;
    }
    return data;
  }
};

static IGeneratedItemPtr CreateGeneratedItem(
    const FileLocationCache &file_location_cache, const VariantEntity &user,
    const VariantEntity &used, RawEntityId aliased_entity_id=kInvalidEntityId) {

  return std::make_shared<CallHierarchyItem>(
      user, aliased_entity_id, NameOfEntity(used),
      LocationOfEntity(file_location_cache, user), EntityBreadCrumbs(user));
}

class CallHierarchyGenerator final : public ITreeGenerator {
  const FileLocationCache file_location_cache;
  const VariantEntity root_entity;

 public:
  virtual ~CallHierarchyGenerator(void) = default;

  inline CallHierarchyGenerator(FileLocationCache file_location_cache_,
                                VariantEntity root_entity_)
      : file_location_cache(std::move(file_location_cache_)),
        root_entity(std::move(root_entity_)) {}

  int NumColumns(void) const Q_DECL_FINAL;

  QString ColumnTitle(int) const Q_DECL_FINAL;

  QString Name(const ITreeGeneratorPtr &self) const Q_DECL_FINAL;

  gap::generator<IGeneratedItemPtr> Roots(
      const ITreeGeneratorPtr &self) Q_DECL_FINAL;

  gap::generator<IGeneratedItemPtr> Children(
      const ITreeGeneratorPtr &self,
      const VariantEntity &parent_entity) Q_DECL_FINAL;
};

int CallHierarchyGenerator::NumColumns(void) const {
  return 3;
}

QString CallHierarchyGenerator::ColumnTitle(int col) const {
  switch (col) {
    case 0: return QObject::tr("Entity");
    case 1: return QObject::tr("File Name");
    case 2: return QObject::tr("Breadcrumbs");
    default: return QString();
  }
}

QString CallHierarchyGenerator::Name(
    const ITreeGeneratorPtr &) const {

  if (auto name = NameOfEntityAsString(root_entity)) {
    return QObject::tr("Call hierarchy of `%1`").arg(name.value());
  } else {
    return QObject::tr("Call hierarchy of entity %1").arg(
        EntityId(root_entity).Pack());
  }
}

gap::generator<IGeneratedItemPtr> CallHierarchyGenerator::Roots(
    const ITreeGeneratorPtr &self) {

  if (std::holds_alternative<Decl>(root_entity)) {
    RawEntityId prev_redecl_id = kInvalidEntityId;
    for (Decl redecl : std::get<Decl>(root_entity).redeclarations()) {
      auto item = CreateGeneratedItem(file_location_cache, redecl, redecl,
                                      prev_redecl_id);
      prev_redecl_id = item->EntityId();
      co_yield item;
    }

  } else {
    co_yield CreateGeneratedItem(file_location_cache, root_entity, root_entity);
  }
}

gap::generator<IGeneratedItemPtr> CallHierarchyGenerator::Children(
    const ITreeGeneratorPtr &self, const VariantEntity &entity) {

  VariantEntity containing_entity = entity;
  if (!Decl::from(entity)) {
    containing_entity = NamedEntityContaining(entity);
  }

  if (std::holds_alternative<NotAnEntity>(containing_entity)) {
    co_return;
  }

  for (Reference ref : Reference::to(containing_entity)) {
    auto use = ref.as_variant();
    auto user = NamedEntityContaining(use);

    if (auto bk = ref.builtin_reference_kind()) {
      if (bk.value() == BuiltinReferenceKind::USES_TYPE) {
        user = use;
      }
    }

    // We might have many uses of a thing, e.g. multiple calls to a function
    // A within a function B, and so we want the Nth call to reference the
    // first call.
    auto aliased_entity_id = kInvalidEntityId;
    if (std::holds_alternative<Decl>(user)) {
      aliased_entity_id =
          std::get<Decl>(user).canonical_declaration().id().Pack();
    }

    co_yield CreateGeneratedItem(file_location_cache, use, user,
                                 aliased_entity_id);
  }
}

}  // namespace

struct CallHierarchyPlugin::PrivateData {
  const Context &context;

  TriggerHandle popup_reference_explorer_trigger;

  inline PrivateData(const Context &context_)
      : context(context_),
        popup_reference_explorer_trigger(context.ActionManager().Find(
            "com.trailofbits.action.OpenReferenceExplorer")) {}
};

CallHierarchyPlugin::~CallHierarchyPlugin(void) {}

CallHierarchyPlugin::CallHierarchyPlugin(
    const Context &context, QObject *parent)
    : IReferenceExplorerPlugin(context, parent),
      d(new PrivateData(context)) {}

std::optional<NamedAction> CallHierarchyPlugin::ActOnMainWindowSecondaryClick(
    QMainWindow *, const QModelIndex &index) {

  VariantEntity entity = IModel::EntitySkipThroughTokens(index);
  if (std::holds_alternative<NotAnEntity>(entity)) {
    return std::nullopt;
  }

  return NamedAction{
    .name = ActionName(entity),
    .action = d->popup_reference_explorer_trigger,
    .data = QVariant::fromValue(std::make_shared<CallHierarchyGenerator>(
        d->context.Index(),
        d->context.FileLocationCache(),
        std::move(entity)))
  };
}

// Allow a main window plugin to act on a key sequence.
std::optional<NamedAction> CallHierarchyPlugin::ActOnMainWindowKeyPress(
    QMainWindow *window, const QKeySequence &keys,
    const QModelIndex &index) {

  if (keys != kKeySeqX) {
    return std::nullopt;
  }

  return ActOnMainWindowSecondaryClick(window, index);
}

}  // namespace mx::gui
