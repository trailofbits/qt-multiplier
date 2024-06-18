// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Plugins/ClassHierarchyPlugin.h>

#include <multiplier/AST/CXXRecordDecl.h>
#include <multiplier/GUI/Interfaces/IModel.h>
#include <multiplier/GUI/Interfaces/ITreeGenerator.h>
#include <multiplier/GUI/Managers/ActionManager.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Util.h>
#include <multiplier/Index.h>

Q_DECLARE_METATYPE(mx::TokenRange);

namespace mx::gui {
namespace {

static const QKeySequence kKeySeqC("C");

static QString ActionName(const VariantEntity &) {
  return QObject::tr("Open Class Hierarchy");
}

class ClassHierarchyItem final : public IGeneratedItem {
  CXXRecordDecl entity;
  TokenRange name_tokens;
  QString location;

 public:
  virtual ~ClassHierarchyItem(void) = default;

  inline ClassHierarchyItem(CXXRecordDecl entity_, TokenRange name_tokens_,
                            QString location_)
      : entity(std::move(entity_)),
        name_tokens(std::move(name_tokens_)),
        location(std::move(location_)) {}

  VariantEntity Entity(void) const Q_DECL_FINAL {
    return entity;
  }

  VariantEntity AliasedEntity(void) const Q_DECL_FINAL {
    return entity;
  }

  QVariant Data(int col) const Q_DECL_FINAL {
    QVariant data;
    switch (col) {
      case 0: data.setValue(name_tokens); break;
      case 1: data.setValue(location); break;
      default: break;
    }
    return data;
  }
};

static IGeneratedItemPtr CreateGeneratedItem(
    const FileLocationCache &file_location_cache,
    const CXXRecordDecl class_) {
  auto name = NameOfEntity(class_);
  auto loc = LocationOfEntity(file_location_cache, class_);
  return std::make_shared<ClassHierarchyItem>(
      std::move(class_), std::move(name), std::move(loc));
}

class ClassHierarchyGenerator final : public ITreeGenerator {
  const FileLocationCache file_location_cache;
  const CXXRecordDecl root_entity;
  const unsigned initialize_expansion_depth;

 public:
  virtual ~ClassHierarchyGenerator(void) = default;

  inline ClassHierarchyGenerator(FileLocationCache file_location_cache_,
                                 CXXRecordDecl root_entity_,
                                 unsigned initialize_expansion_depth_=2u)
      : file_location_cache(std::move(file_location_cache_)),
        root_entity(root_entity_.canonical_declaration()),
        initialize_expansion_depth(initialize_expansion_depth_) {}

  unsigned InitialExpansionDepth(void) const Q_DECL_FINAL;

  int NumColumns(void) const Q_DECL_FINAL;

  int SortColumn(void) const Q_DECL_FINAL;

  QString ColumnTitle(int) const Q_DECL_FINAL;

  QString Name(const ITreeGeneratorPtr &self) const Q_DECL_FINAL;

  gap::generator<IGeneratedItemPtr> Roots(
      ITreeGeneratorPtr self) Q_DECL_FINAL;

  gap::generator<IGeneratedItemPtr> Children(
      ITreeGeneratorPtr self, IGeneratedItemPtr parent_item) Q_DECL_FINAL;
};

unsigned ClassHierarchyGenerator::InitialExpansionDepth(void) const {
  return initialize_expansion_depth;
}

int ClassHierarchyGenerator::NumColumns(void) const {
  return 2;
}

int ClassHierarchyGenerator::SortColumn(void) const {
  return 1;  // The breadcrumbs column.
}

QString ClassHierarchyGenerator::ColumnTitle(int col) const {
  switch (col) {
    case 0: return QObject::tr("Class");
    case 1: return QObject::tr("File Name");
    default: return QString();
  }
}

QString ClassHierarchyGenerator::Name(
    const ITreeGeneratorPtr &) const {
  auto name = NameOfEntityAsString(root_entity);
  if (name) {
    return QObject::tr("Class hierarchy of `%1`").arg(name.value());
  } else {
    return QObject::tr("Class hierarchy of entity %1").arg(
        root_entity.id().Pack());
  }
}

gap::generator<IGeneratedItemPtr> ClassHierarchyGenerator::Roots(
    ITreeGeneratorPtr self) {
  co_yield CreateGeneratedItem(file_location_cache, root_entity);
}

gap::generator<IGeneratedItemPtr> ClassHierarchyGenerator::Children(
    ITreeGeneratorPtr self, IGeneratedItemPtr parent_item) {

  VariantEntity entity = parent_item->Entity();
  auto parent_class = CXXRecordDecl::from(entity);
  if (!parent_class) {
    co_return;
  }

  auto derived_classes = parent_class->derived_classes();
  for (auto derived_class : derived_classes) {
    co_yield CreateGeneratedItem(
        file_location_cache, std::move(derived_class));
  }
}

}  // namespace

struct ClassHierarchyPlugin::PrivateData {
  const ConfigManager &config_manager;

  TriggerHandle open_reference_explorer_trigger;

  inline PrivateData(const ConfigManager &config_manager_)
      : config_manager(config_manager_),
        open_reference_explorer_trigger(config_manager.ActionManager().Find(
            "com.trailofbits.action.OpenReferenceExplorer")) {}
};

ClassHierarchyPlugin::~ClassHierarchyPlugin(void) {}

ClassHierarchyPlugin::ClassHierarchyPlugin(
    ConfigManager &config_manager, QObject *parent)
    : IReferenceExplorerPlugin(config_manager, parent),
      d(new PrivateData(config_manager)) {}

std::optional<NamedAction> ClassHierarchyPlugin::ActOnSecondaryClick(
    IWindowManager *, const QModelIndex &index) {

  VariantEntity entity = IModel::EntitySkipThroughTokens(index);

  // It's only reasonable to ask for class hierarchy info on classes.
  auto record = CXXRecordDecl::from(entity);
  if (!record) {
    return std::nullopt;
  }

  return NamedAction{
    .name = ActionName(entity),
    .action = d->open_reference_explorer_trigger,
    .data = QVariant::fromValue<ITreeGeneratorPtr>(
        std::make_shared<ClassHierarchyGenerator>(
            d->config_manager.FileLocationCache(),
            std::move(record.value())))
  };
}

// Allow a main window plugin to act on a key sequence.
std::optional<NamedAction> ClassHierarchyPlugin::ActOnKeyPress(
    IWindowManager *manager, const QKeySequence &keys,
    const QModelIndex &index) {

  if (keys != kKeySeqC) {
    return std::nullopt;
  }

  return ActOnSecondaryClick(manager, index);
}

}  // namespace mx::gui
