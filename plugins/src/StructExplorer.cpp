// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/AST/ConstantArrayType.h>
#include <multiplier/AST/RecordDecl.h>
#include <multiplier/AST/RecordType.h>
#include <multiplier/Frontend/Token.h>
#include <multiplier/Frontend/TokenCategory.h>
#include <multiplier/GUI/Plugins/StructExplorerPlugin.h>

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
#include <memory>
#include <sstream>
#include <variant>

Q_DECLARE_METATYPE(mx::TokenRange);

namespace mx::gui {
namespace {

static const QKeySequence kKeySeqX("S");

static QString ActionName(const VariantEntity &) {
  return QObject::tr("Open Struct Explorer");
}

static TokenRange BitsToTokenRange(uint64_t num_bits) {
  std::stringstream ss;
  ss << num_bits / 8u;
  if (num_bits % 8 != 0) {
    ss << "." << num_bits % 8u;
  }
  std::vector<CustomToken> toks;
  UserToken tok;
  tok.category = TokenCategory::LITERAL;
  tok.kind = TokenKind::NUMERIC_CONSTANT;
  tok.data = ss.str();
  toks.emplace_back(std::move(tok));
  tok.category = TokenCategory::WHITESPACE;
  tok.kind = TokenKind::WHITESPACE;
  tok.data = " ";
  toks.emplace_back(std::move(tok));
  return TokenRange::create(std::move(toks));
}

class StructExplorerItem final : public IGeneratedItem {
  VariantEntity entity;
  VariantEntity aliased_entity;
  TokenRange name_tokens;
  QString location;
  std::optional<TokenRange> type_tokens;
  std::optional<uint64_t> offset_in_bits;
  std::optional<uint64_t> cumulative_offset_in_bits;
  std::optional<uint64_t> size_in_bits;

 public:
  virtual ~StructExplorerItem(void) = default;

  inline StructExplorerItem(VariantEntity entity_,
                            VariantEntity aliased_entity_,
                            TokenRange name_tokens_, QString location_,
                            std::optional<TokenRange> type_tokens_,
                            std::optional<uint64_t> offset_in_bits_,
                            std::optional<uint64_t> cumulative_offset_in_bits_,
                            std::optional<uint64_t> size_in_bits_)
      : entity(std::move(entity_)),
        aliased_entity(std::move(aliased_entity_)),
        name_tokens(std::move(name_tokens_)),
        location(std::move(location_)),
        type_tokens(std::move(type_tokens_)),
        offset_in_bits(std::move(offset_in_bits_)),
        cumulative_offset_in_bits(std::move(cumulative_offset_in_bits_)),
        size_in_bits(std::move(size_in_bits_)) {}

  VariantEntity Entity(void) const Q_DECL_FINAL {
    return entity;
  }

  VariantEntity AliasedEntity(void) const Q_DECL_FINAL {
    return aliased_entity;
  }

  std::optional<uint64_t> CumulativeOffsetInBits(void) const {
    return cumulative_offset_in_bits;
  }

  QVariant Data(int col) const Q_DECL_FINAL {
    QVariant data;
    switch (col) {
      case 0:
        if (offset_in_bits) {
          data.setValue(BitsToTokenRange(*offset_in_bits));
        }
        break;
      case 1:
        if (cumulative_offset_in_bits) {
          data.setValue(BitsToTokenRange(*cumulative_offset_in_bits));
        }
        break;
      case 2: data.setValue(name_tokens); break;
      case 3:
        if (type_tokens) {
          data.setValue(InjectWhitespace(*type_tokens));
        }
        break;
      case 4:
        if (size_in_bits) {
          data.setValue(BitsToTokenRange(*size_in_bits));
        }
        break;
      default: break;
    }
    return data;
  }
};

class StructExplorerGenerator final : public ITreeGenerator {
  const FileLocationCache file_location_cache;
  const VariantEntity root_entity;

 public:
  virtual ~StructExplorerGenerator(void) = default;

  inline StructExplorerGenerator(FileLocationCache file_location_cache_,
                                 VariantEntity root_entity_)
      : file_location_cache(std::move(file_location_cache_)),
        root_entity(std::move(root_entity_)) {}

  int SortColumn(void) const;
  bool EnableDeduplication(void) const Q_DECL_FINAL;
  int NumColumns(void) const Q_DECL_FINAL;
  QString ColumnTitle(int) const Q_DECL_FINAL;
  QString Name(const ITreeGeneratorPtr &self) const Q_DECL_FINAL;

  gap::generator<IGeneratedItemPtr> Roots(ITreeGeneratorPtr self) Q_DECL_FINAL;

  gap::generator<IGeneratedItemPtr>
  Children(ITreeGeneratorPtr self, IGeneratedItemPtr parent_item) Q_DECL_FINAL;
};

int StructExplorerGenerator::SortColumn(void) const {
  return -1;
}

int StructExplorerGenerator::NumColumns(void) const {
  return 5;
}

bool StructExplorerGenerator::EnableDeduplication(void) const {
  return false;
}

QString StructExplorerGenerator::Name(const ITreeGeneratorPtr &) const {
  auto name = NameOfEntityAsString(root_entity);
  if (name) {
    return QObject::tr("Struct of `%1`").arg(name.value());
  } else {
    return QObject::tr("Struct of entity %1").arg(EntityId(root_entity).Pack());
  }
}

QString StructExplorerGenerator::ColumnTitle(int col) const {
  switch (col) {
    case 0: return QObject::tr("Offset");
    case 1: return QObject::tr("Cumulative Offset");
    case 2: return QObject::tr("Name");
    case 3: return QObject::tr("Type");
    case 4: return QObject::tr("Size");
    default: return QString();
  }
}

static IGeneratedItemPtr
CreateGeneratedItem(const FileLocationCache &file_location_cache,
                    const VariantEntity &entity, TokenRange name,
                    std::optional<TokenRange> type_tokens,
                    std::optional<uint64_t> offset_in_bits,
                    std::optional<uint64_t> cumulative_offset_in_bits,
                    std::optional<uint64_t> size_in_bits) {

  return std::make_shared<StructExplorerItem>(
      entity, entity, name, LocationOfEntity(file_location_cache, entity),
      type_tokens, offset_in_bits, cumulative_offset_in_bits, size_in_bits);
}

gap::generator<IGeneratedItemPtr>
StructExplorerGenerator::Roots(ITreeGeneratorPtr self) {
  // It's only reasonable to ask for references to named entities.
  if (!RecordDecl::from(root_entity)) {
    co_return;
  }
  qDebug() << "Roots!!";
  auto rd = RecordDecl::from(root_entity);
  rd = rd->canonical_declaration();
  for (const auto &field : rd->fields()) {
    co_yield CreateGeneratedItem(file_location_cache, field.type(),
                                 NameOfEntity(field, /*qualified=*/false),
                                 field.type().tokens(), field.offset_in_bits(),
                                 field.offset_in_bits(),
                                 field.type().size_in_bits());
  }
}

static TokenRange IndexTokenRange(uint64_t index) {
  std::vector<CustomToken> toks;
  UserToken tok;
  tok.category = TokenCategory::PUNCTUATION;
  tok.kind = TokenKind::L_SQUARE;
  tok.data = "[";
  toks.emplace_back(std::move(tok));
  tok.category = TokenCategory::LITERAL;
  tok.kind = TokenKind::NUMERIC_CONSTANT;
  tok.data = std::to_string(index);
  toks.emplace_back(std::move(tok));
  tok.category = TokenCategory::PUNCTUATION;
  tok.kind = TokenKind::R_SQUARE;
  tok.data = "]";
  toks.emplace_back(std::move(tok));
  return TokenRange::create(std::move(toks));
}

gap::generator<IGeneratedItemPtr>
StructExplorerGenerator::Children(ITreeGeneratorPtr self,
                                  IGeneratedItemPtr parent_item) {
  auto item = std::dynamic_pointer_cast<const StructExplorerItem>(parent_item);
  assert(item != nullptr);
  auto entity = item->Entity();

  qDebug() << "Children!";

  if (auto ty = Type::from(entity)) {
    qDebug() << "Type " << ty->tokens().data();
  }

  if (auto rt = RecordType::from(entity)) {
    qDebug() << "Record";
    auto rd = RecordDecl::from(rt->declaration());
    for (const auto &rd_field : rd->fields()) {
      std::optional<uint64_t> new_offset;
      if (item->CumulativeOffsetInBits() && rd_field.offset_in_bits()) {
        new_offset = rd_field.offset_in_bits().value() +
                     item->CumulativeOffsetInBits().value();
      }
      co_yield CreateGeneratedItem(file_location_cache, rd_field.type(),
                                   NameOfEntity(rd_field, /*qualified=*/false),
                                   rd_field.type().tokens(),
                                   rd_field.offset_in_bits(), new_offset,
                                   rd_field.type().size_in_bits());
    }
  } else if (auto at = ConstantArrayType::from(entity)) {
    qDebug() << "Constant Array";
    auto elem_size = at->element_type().size_in_bits().value();
    auto num_elems = at->size_in_bits().value() / elem_size;
    qDebug() << "Number of elements: " << num_elems;
    co_yield CreateGeneratedItem(
        file_location_cache, at->element_type(), IndexTokenRange(num_elems),
        at->element_type().tokens(), 0, item->CumulativeOffsetInBits(),
        at->size_in_bits());
  }
}

}  // namespace

struct StructExplorerPlugin::PrivateData {
  const ConfigManager &config_manager;

  TriggerHandle open_struct_explorer_trigger;

  inline PrivateData(const ConfigManager &config_manager_)
      : config_manager(config_manager_),
        open_struct_explorer_trigger(config_manager.ActionManager().Find(
            "com.trailofbits.action.OpenReferenceExplorer")) {}
};

StructExplorerPlugin::~StructExplorerPlugin(void) {}

StructExplorerPlugin::StructExplorerPlugin(ConfigManager &config_manager,
                                           QObject *parent)
    : IReferenceExplorerPlugin(config_manager, parent),
      d(new PrivateData(config_manager)) {}

std::optional<NamedAction>
StructExplorerPlugin::ActOnSecondaryClick(IWindowManager *,
                                          const QModelIndex &index) {

  VariantEntity entity = IModel::EntitySkipThroughTokens(index);

  // It's only reasonable to ask for references to named entities.
  if (!NamedDecl::from(entity) && !RecordDecl::from(entity) &&
      !File::from(entity)) {
    return std::nullopt;
  }

  return NamedAction{
      .name = ActionName(entity),
      .action = d->open_struct_explorer_trigger,
      .data = QVariant::fromValue<ITreeGeneratorPtr>(
          std::make_shared<StructExplorerGenerator>(
              d->config_manager.FileLocationCache(), std::move(entity)))};
}

// Allow a main window plugin to act on a key sequence.
std::optional<NamedAction>
StructExplorerPlugin::ActOnKeyPress(IWindowManager *, const QKeySequence &keys,
                                    const QModelIndex &index) {

  VariantEntity entity = IModel::EntitySkipThroughTokens(index);

  // It's only reasonable to ask for references to named entities.
  if (!DefineMacroDirective::from(entity) && !MacroParameter::from(entity) &&
      !NamedDecl::from(entity) && !File::from(entity)) {
    return std::nullopt;
  }

  QString action_name;

  if (keys == kKeySeqX) {
    action_name = ActionName(entity);
  }

  if (action_name.isEmpty()) {
    return std::nullopt;
  }

  return NamedAction{
      .name = action_name,
      .action = d->open_struct_explorer_trigger,
      .data = QVariant::fromValue<ITreeGeneratorPtr>(
          std::make_shared<StructExplorerGenerator>(
              d->config_manager.FileLocationCache(), std::move(entity)))};
}

}  // namespace mx::gui
