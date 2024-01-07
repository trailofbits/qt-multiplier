// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Plugins/CallHierarchyPlugin.h>

#include <multiplier/AST/NamedDecl.h>
#include <multiplier/GUI/Interfaces/IModel.h>
#include <multiplier/GUI/Interfaces/ITreeGenerator.h>
#include <multiplier/GUI/Managers/ActionManager.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Util.h>
#include <multiplier/Frontend/DefineMacroDirective.h>
#include <multiplier/Frontend/MacroParameter.h>
#include <multiplier/Frontend/File.h>
#include <multiplier/Index.h>

Q_DECLARE_METATYPE(mx::TokenRange);

namespace mx::gui {
namespace {

static const QKeySequence kKeySeqX("X");
static const QKeySequence kExpandSequences[] = {
  {"1"}, {"2"}, {"3"}, {"4"}, {"5"}, {"6"}, {"7"}, {"8"}, {"9"}
};

static QString ActionName(const VariantEntity &) {
  return QObject::tr("Open Call Hierarchy");
}

class CallHierarchyItem final : public IGeneratedItem {

  VariantEntity user_entity;
  VariantEntity used_entity;
  TokenRange name_tokens;
  QString location;
  QString breadcrumbs;

 public:
  virtual ~CallHierarchyItem(void) = default;

  inline CallHierarchyItem(VariantEntity user_entity_,
                           VariantEntity used_entity_,
                           TokenRange name_tokens_, QString location_,
                           QString breadcrumbs_)
      : user_entity(std::move(user_entity_)),
        used_entity(used_entity_),
        name_tokens(std::move(name_tokens_)),
        location(std::move(location_)),
        breadcrumbs(std::move(breadcrumbs_)) {}

  VariantEntity Entity(void) const Q_DECL_FINAL {
    return user_entity;
  }

  VariantEntity AliasedEntity(void) const Q_DECL_FINAL {
    return used_entity;
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
    const FileLocationCache &file_location_cache,
    const VariantEntity &user, const VariantEntity &used) {

  return std::make_shared<CallHierarchyItem>(
      user, used, NameOfEntity(used),
      LocationOfEntity(file_location_cache, user), EntityBreadCrumbs(user));
}

class CallHierarchyGenerator final : public ITreeGenerator {
  const FileLocationCache file_location_cache;
  const VariantEntity root_entity;
  const unsigned initialize_expansion_depth;

 public:
  virtual ~CallHierarchyGenerator(void) = default;

  inline CallHierarchyGenerator(FileLocationCache file_location_cache_,
                                VariantEntity root_entity_,
                                unsigned initialize_expansion_depth_=2u)
      : file_location_cache(std::move(file_location_cache_)),
        root_entity(std::move(root_entity_)),
        initialize_expansion_depth(initialize_expansion_depth_) {}

  unsigned InitialExpansionDepth(void) const Q_DECL_FINAL;

  int NumColumns(void) const Q_DECL_FINAL;

  QString ColumnTitle(int) const Q_DECL_FINAL;

  QString Name(const ITreeGeneratorPtr &self) const Q_DECL_FINAL;

  gap::generator<IGeneratedItemPtr> Roots(
      ITreeGeneratorPtr self) Q_DECL_FINAL;

  gap::generator<IGeneratedItemPtr> Children(
      ITreeGeneratorPtr self, IGeneratedItemPtr parent_item) Q_DECL_FINAL;
};

unsigned CallHierarchyGenerator::InitialExpansionDepth(void) const {
  return initialize_expansion_depth;
}

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

  auto name = NameOfEntityAsString(root_entity);
  if (std::holds_alternative<File>(root_entity)) {
    for (auto path : std::get<File>(root_entity).paths()) {
      name = QString::fromStdString(path.filename().generic_string());
      break;
    }
  }

  if (name) {
    return QObject::tr("Call hierarchy of `%1`").arg(name.value());
  } else {
    return QObject::tr("Call hierarchy of entity %1").arg(
        EntityId(root_entity).Pack());
  }
}

gap::generator<IGeneratedItemPtr> CallHierarchyGenerator::Roots(
    ITreeGeneratorPtr self) {
  if (std::holds_alternative<Decl>(root_entity)) {
    std::optional<Decl> decl;
    for (Decl redecl : std::get<Decl>(root_entity).redeclarations()) {
      if (!decl) {
        decl = redecl;
      }
      auto item = CreateGeneratedItem(file_location_cache, redecl, decl.value());
      co_yield item;
    }

  } else {
    co_yield CreateGeneratedItem(file_location_cache, root_entity, root_entity);
  }
}

gap::generator<IGeneratedItemPtr> CallHierarchyGenerator::Children(
    ITreeGeneratorPtr self, IGeneratedItemPtr parent_item) {

  VariantEntity entity = parent_item->AliasedEntity();
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
    if (std::holds_alternative<Decl>(user)) {
      user = std::get<Decl>(user).canonical_declaration();
    }

    // NOTE(pag): `use` is a *user* of `containing_entity`, and `user` is a
    //            use of (really, container of) `use`.
    co_yield CreateGeneratedItem(file_location_cache, use, user);
  }
}

}  // namespace

struct CallHierarchyPlugin::PrivateData {
  const ConfigManager &config_manager;

  TriggerHandle open_reference_explorer_trigger;

  inline PrivateData(const ConfigManager &config_manager_)
      : config_manager(config_manager_),
        open_reference_explorer_trigger(config_manager.ActionManager().Find(
            "com.trailofbits.action.OpenReferenceExplorer")) {}
};

CallHierarchyPlugin::~CallHierarchyPlugin(void) {}

CallHierarchyPlugin::CallHierarchyPlugin(
    ConfigManager &config_manager, QObject *parent)
    : IReferenceExplorerPlugin(config_manager, parent),
      d(new PrivateData(config_manager)) {}

std::optional<NamedAction> CallHierarchyPlugin::ActOnMainWindowSecondaryClick(
    IWindowManager *, const QModelIndex &index) {

  VariantEntity entity = IModel::EntitySkipThroughTokens(index);

  // It's only reasonable to ask for references to named entities.
  if (!DefineMacroDirective::from(entity) && !MacroParameter::from(entity) &&
      !NamedDecl::from(entity) && !File::from(entity)) {
    return std::nullopt;
  }

  return NamedAction{
    .name = ActionName(entity),
    .action = d->open_reference_explorer_trigger,
    .data = QVariant::fromValue<ITreeGeneratorPtr>(
        std::make_shared<CallHierarchyGenerator>(
            d->config_manager.FileLocationCache(), std::move(entity)))
  };
}

// Allow a main window plugin to act on a key sequence.
std::optional<NamedAction> CallHierarchyPlugin::ActOnMainWindowKeyPress(
    IWindowManager *, const QKeySequence &keys, const QModelIndex &index) {

  VariantEntity entity = IModel::EntitySkipThroughTokens(index);

  // It's only reasonable to ask for references to named entities.
  if (!DefineMacroDirective::from(entity) && !MacroParameter::from(entity) &&
      !NamedDecl::from(entity) && !File::from(entity)) {
    return std::nullopt;
  }

  QString action_name;

  auto depth = 1u;
  if (keys == kKeySeqX) {
    action_name = ActionName(entity);
  
  } else {
    for (auto expand_seq : kExpandSequences) {
      if (keys != expand_seq) {
        ++depth;
        continue;
      }

      action_name = tr("Open Call Hierarchy (Depth %1)").arg(depth);
      break;
    }
  }

  if (action_name.isEmpty()) {
    return std::nullopt;
  }

  return NamedAction{
    .name = action_name,
    .action = d->open_reference_explorer_trigger,
    .data = QVariant::fromValue<ITreeGeneratorPtr>(
        std::make_shared<CallHierarchyGenerator>(
            d->config_manager.FileLocationCache(), std::move(entity),
            depth + 1u  /* logical depth 1 is physcal depth 1,
                         * i.e. 1 under a root */))
  };
}

}  // namespace mx::gui
