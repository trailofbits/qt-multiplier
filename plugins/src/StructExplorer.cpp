// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/AST/ConstantArrayType.h>
#include <multiplier/AST/CXXBaseSpecifier.h>
#include <multiplier/AST/CXXRecordDecl.h>
#include <multiplier/AST/RecordType.h>
#include <multiplier/AST/TypedefNameDecl.h>
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
#include <multiplier/Index.h>
#include <memory>
#include <sstream>
#include <variant>

Q_DECLARE_METATYPE(mx::TokenRange);

namespace mx::gui {
namespace {

static const QKeySequence kKeySeqS("S");

static QString ActionName(const VariantEntity &) {
  return QObject::tr("Open Struct Explorer");
}

class StructExplorerItem final : public IGeneratedItem {
  VariantEntity entity;
  VariantEntity aliased_entity;
  TokenRange name_tokens;
  TokenRange type_tokens;
  std::optional<TokenRange> offset_tokens;
  std::optional<TokenRange> cumulative_offset_tokens;
  std::optional<TokenRange> size_tokens;
  std::optional<uint64_t> cumulative_offset_bits;

 public:
  virtual ~StructExplorerItem(void) = default;

  inline StructExplorerItem(VariantEntity entity_,
                            VariantEntity aliased_entity_,
                            TokenRange name_tokens_,
                            TokenRange type_tokens_,
                            std::optional<TokenRange> offset_tokens_,
                            std::optional<TokenRange> cumulative_offset_tokens_,
                            std::optional<TokenRange> size_tokens_,
                            std::optional<uint64_t> cumulative_offset_bits_)
      : entity(std::move(entity_)),
        aliased_entity(std::move(aliased_entity_)),
        name_tokens(std::move(name_tokens_)),
        type_tokens(std::move(type_tokens_)),
        offset_tokens(std::move(offset_tokens_)),
        cumulative_offset_tokens(std::move(cumulative_offset_tokens_)),
        size_tokens(std::move(size_tokens_)),
        cumulative_offset_bits(std::move(cumulative_offset_bits_)) {}

  VariantEntity Entity(void) const Q_DECL_FINAL {
    return entity;
  }

  VariantEntity AliasedEntity(void) const Q_DECL_FINAL {
    return aliased_entity;
  }

  std::optional<uint64_t> CumulativeOffsetInBits(void) const {
    return cumulative_offset_bits;
  }

  QVariant Data(int col) const Q_DECL_FINAL {
    QVariant data;
    switch (col) {
      case 0:
        if (offset_tokens) {
          data.setValue(*offset_tokens);
        }
        break;
      case 1:
        if (cumulative_offset_tokens) {
          data.setValue(*cumulative_offset_tokens);
        }
        break;
      case 2:
        if (size_tokens) {
          data.setValue(*size_tokens);
        }
        break;
      case 3: data.setValue(name_tokens); break;
      case 4:
        data.setValue(type_tokens);
        break;
      default: break;
    }
    return data;
  }
};

class StructExplorerGenerator final : public ITreeGenerator {
  const RecordDecl root_entity;

 public:
  virtual ~StructExplorerGenerator(void) = default;

  inline StructExplorerGenerator(RecordDecl root_entity_)
      : root_entity(std::move(root_entity_)) {}

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
    return QObject::tr("Struct of entity %1").arg(root_entity.id().Pack());
  }
}

QString StructExplorerGenerator::ColumnTitle(int col) const {
  switch (col) {
    case 0: return QObject::tr("Offset");
    case 1: return QObject::tr("Cumulative");
    case 2: return QObject::tr("Size");
    case 3: return QObject::tr("Name");
    case 4: return QObject::tr("Type");
    default: return QString();
  }
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

static IGeneratedItemPtr
CreateGeneratedItem(const VariantEntity &entity, TokenRange name,
                    TokenRange type_tokens,
                    std::optional<uint64_t> offset_in_bits,
                    std::optional<uint64_t> cumulative_offset_bits,
                    std::optional<uint64_t> size_in_bits) {
  type_tokens = InjectWhitespace(type_tokens);

  std::optional<TokenRange> offset_tokens;
  if (offset_in_bits) {
    offset_tokens = BitsToTokenRange(*offset_in_bits);
  }

  std::optional<TokenRange> cumulative_offset_tokens;
  if (cumulative_offset_bits) {
    cumulative_offset_tokens = BitsToTokenRange(*cumulative_offset_bits);
  }

  std::optional<TokenRange> size_tokens;
  if (size_in_bits) {
    size_tokens = BitsToTokenRange(*size_in_bits);
  }

  return std::make_shared<StructExplorerItem>(
      entity, entity, name, type_tokens, offset_tokens,
      cumulative_offset_tokens, size_tokens, cumulative_offset_bits);
}

struct SizeOffsetAndBase {
  uint64_t size;
  uint64_t offset;
  CXXBaseSpecifier spec;
  CXXRecordDecl record;

  SizeOffsetAndBase(void) = delete;

  inline SizeOffsetAndBase(uint64_t size_, uint64_t offset_,
                           CXXBaseSpecifier spec_,
                           CXXRecordDecl record_)
      : size(size_),
        offset(offset_),
        spec(std::move(spec_)),
        record(std::move(record_)) {}
};

static std::vector<SizeOffsetAndBase> GetBases(RecordDecl &rd) {

  std::vector<SizeOffsetAndBase> bases;
  auto cls = CXXRecordDecl::from(rd);
  if (!cls) {
    return bases;
  }

  auto base_specifiers = cls->bases();
  if (!base_specifiers) {
    return bases;
  }
  for (auto &base : *base_specifiers) {
    auto base_size = base.base_type().size_in_bits();
    if (!base_size) {
      continue;
    }

    auto base_cls = base.base_class();
    if (!base_cls) {
      continue;
    }

    auto base_cls_offset = base.offset_in_bits();
    if (!base_cls_offset) {
      continue;
    }

    bases.emplace_back(base_size.value(),
                       base_cls_offset.value(),
                       std::move(base),
                       std::move(base_cls.value()));
  }

  // Sort the base classes by offset.
  std::sort(bases.begin(), bases.end(),
            [] (const SizeOffsetAndBase &a, const SizeOffsetAndBase &b) {
              return a.offset < b.offset;
            });

  return bases;
}

gap::generator<IGeneratedItemPtr>
StructExplorerGenerator::Roots(ITreeGeneratorPtr self) {
  auto rd = root_entity;
  // If the struct already has a definition, use the one
  // that is being asked for, otherwise use the canonical
  // definition
  if (!rd.is_definition()) {
    rd = rd.canonical_declaration();
  }

  for (const auto &offset_base : GetBases(rd)) {
    co_yield CreateGeneratedItem(
        offset_base.spec,
        TokenRange(),
        NameOfEntity(offset_base.record, /*qualified=*/true),
        offset_base.offset, offset_base.offset, offset_base.size);
  }

  for (const auto &field : rd.fields()) {
    co_yield CreateGeneratedItem(
        field, NameOfEntity(field, /*qualified=*/false),
        field.type().tokens(), field.offset_in_bits(), field.offset_in_bits(),
        field.type().size_in_bits());
  }
}

gap::generator<IGeneratedItemPtr>
StructExplorerGenerator::Children(ITreeGeneratorPtr self,
                                  IGeneratedItemPtr parent_item) {
  auto item = std::dynamic_pointer_cast<const StructExplorerItem>(parent_item);
  assert(item != nullptr);
  auto entity = item->Entity();

  auto rd = RecordDecl::from(entity);
  if (auto fd = FieldDecl::from(entity)) {
    auto ty = fd->type().desugared_type();
    if (auto rt = RecordType::from(ty)) {
      rd = RecordDecl::from(rt->declaration());
    }
    if (auto at = ArrayType::from(ty)) {
      auto elem_ty = at->element_type().desugared_type();
      if (auto rt = RecordType::from(elem_ty)) {
        rd = RecordDecl::from(rt->declaration());
      }
    }
  } else if (auto spec = CXXBaseSpecifier::from(entity)) {
    rd = spec->base_class();
  }

  if (!rd) {
    co_return;
  }

  for (const auto &offset_base : GetBases(rd.value())) {
    std::optional<uint64_t> new_offset;
    if (item->CumulativeOffsetInBits()) {
      new_offset = offset_base.offset +
                   item->CumulativeOffsetInBits().value();
    }
    co_yield CreateGeneratedItem(
        offset_base.spec,
        TokenRange(),
        NameOfEntity(offset_base.record, /*qualified=*/true),
        offset_base.offset, new_offset, offset_base.size);
  }

  for (const auto &rd_field : rd->fields()) {
    std::optional<uint64_t> new_offset;
    if (item->CumulativeOffsetInBits() && rd_field.offset_in_bits()) {
      new_offset = rd_field.offset_in_bits().value() +
                   item->CumulativeOffsetInBits().value();
    }
    co_yield CreateGeneratedItem(
        rd_field, NameOfEntity(rd_field, /*qualified=*/false),
        rd_field.type().tokens(), rd_field.offset_in_bits(), new_offset,
        rd_field.type().size_in_bits());
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

static std::optional<RecordDecl> GetRecordDecl(VariantEntity entity) {
  if (auto td = TypedefNameDecl::from(entity)) {
    auto ty = td->underlying_type().desugared_type();
    if (auto rt = RecordType::from(ty)) {
      return RecordDecl::from(rt->declaration());
    }
  }
  if (auto fd = FieldDecl::from(entity)) {
    if (auto pd = fd->parent_declaration()) {
      return RecordDecl::from(pd.value());
    }
  }
  if (auto spec = CXXBaseSpecifier::from(entity)) {
    return spec->base_class();
  }
  return RecordDecl::from(entity);
}

StructExplorerPlugin::StructExplorerPlugin(ConfigManager &config_manager,
                                           QObject *parent)
    : IReferenceExplorerPlugin(config_manager, parent),
      d(new PrivateData(config_manager)) {}

std::optional<NamedAction>
StructExplorerPlugin::ActOnSecondaryClick(IWindowManager *,
                                          const QModelIndex &index) {
  VariantEntity entity = IModel::EntitySkipThroughTokens(index);

  // Struct explorer only works on records
  auto rd = GetRecordDecl(entity);
  if (!rd) {
    return std::nullopt;
  }

  return NamedAction{
      .name = ActionName(entity),
      .action = d->open_struct_explorer_trigger,
      .data = QVariant::fromValue<ITreeGeneratorPtr>(
          std::make_shared<StructExplorerGenerator>(std::move(rd.value())))};
}

// Allow a main window plugin to act on a key sequence.
std::optional<NamedAction>
StructExplorerPlugin::ActOnKeyPress(IWindowManager *, const QKeySequence &keys,
                                    const QModelIndex &index) {
  if (keys != kKeySeqS) {
    return std::nullopt;
  }

  VariantEntity entity = IModel::EntitySkipThroughTokens(index);

  // Struct explorer only works on records
  auto rd = GetRecordDecl(entity);
  if (!rd) {
    return std::nullopt;
  }

  return NamedAction{
      .name = ActionName(entity),
      .action = d->open_struct_explorer_trigger,
      .data = QVariant::fromValue<ITreeGeneratorPtr>(
          std::make_shared<StructExplorerGenerator>(std::move(rd.value())))};
}

}  // namespace mx::gui
