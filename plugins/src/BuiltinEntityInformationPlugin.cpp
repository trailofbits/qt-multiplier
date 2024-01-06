// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Plugins/BuiltinEntityInformationPlugin.h>

#include <multiplier/AST/CXXMethodDecl.h>
#include <multiplier/AST/DeclKind.h>
#include <multiplier/AST/EnumDecl.h>
#include <multiplier/AST/EnumConstantDecl.h>
#include <multiplier/AST/FieldDecl.h>
#include <multiplier/AST/RecordDecl.h>
#include <multiplier/AST/VarDecl.h>
#include <multiplier/Frontend/MacroExpansion.h>
#include <multiplier/Frontend/MacroParameter.h>
#include <multiplier/GUI/Util.h>
#include <multiplier/Index.h>

#include <memory>
#include <sstream>

namespace mx::gui {
namespace {

// Fill the location entry in an generated item.
static void FillLocation(const FileLocationCache &file_location_cache,
                         IInfoGenerator::Item &item) {
  
  // Start by using the tokens. If we can, we use the tokens because sometimes
  // we use some "higher level" context than the specific entity. E.g. the
  // entity is a `DeclRefExpr`, but the higher level context is a `CallExpr`
  // that contains the `DeclRefExpr`.
  for (Token tok : item.tokens.file_tokens()) {
    auto file = File::containing(tok);
    if (!file) {
      continue;
    }

    if (auto line_col = tok.location(file_location_cache)) {
      for (auto path : file->paths()) {
        item.location = 
            QString("%1:%2:%3")
                .arg(QString::fromStdString(path.generic_string()))
                .arg(line_col->first)
                .arg(line_col->second);
        return;
      }
    }
  }

  // Backup path: get the entity's location.
  item.location = LocationOfEntity(file_location_cache, item.entity);
  if (!item.location.isEmpty()) {
    return;
  }

  // Final backup: just use the entity ID.
  item.location = QObject::tr("Entity ID: %1").arg(EntityId(item.entity).Pack());
}

// Generates information about `T`s.
template <typename T>
class EntityInfoGenerator Q_DECL_FINAL : public IInfoGenerator {
 public:
  const T entity;

  virtual ~EntityInfoGenerator(void) = default;

  inline EntityInfoGenerator(T entity_)
      : entity(std::move(entity_)) {}

  gap::generator<IInfoGenerator::Item> Items(
      IInfoGeneratorPtr, FileLocationCache file_location_cache) Q_DECL_FINAL;
};

// Generate information about records. This primarily focuses on fields and
// their byte offsets.
template <>
gap::generator<IInfoGenerator::Item> EntityInfoGenerator<RecordDecl>::Items(
    IInfoGeneratorPtr, FileLocationCache file_location_cache) {

  uint64_t max_offset = 0u;
  uint64_t all_offset = 0u;
  for (const mx::Decl &decl : entity.declarations_in_context()) {
    if (auto fd = mx::FieldDecl::from(decl)) {
      if (auto offset = fd->offset_in_bits()) {
        all_offset |= offset.value();
        max_offset = std::max(max_offset, offset.value());
      }
    }
  }

  auto has_bits = all_offset % 8u;
  auto num_bytes = max_offset / 8u;
  int num_digits = 0;
  for (num_bytes += 9; num_bytes; num_bytes /= 10u) {
    ++num_digits;
  }

  std::vector<CustomToken> toks;
  UserToken tok;
  IInfoGenerator::Item item;

  // Find the local variables.
  for (const mx::Decl &decl : entity.declarations_in_context()) {

    // Var decls.
    if (auto vd = VarDecl::from(decl)) {
      if (vd->tsc_spec() != ThreadStorageClassSpecifier::UNSPECIFIED) {
        item.category = QObject::tr("Thread Local Variables");

      } else {
        item.category = QObject::tr("Global Variables");
      }

      item.tokens = vd->token();
      item.entity = std::move(vd.value());
      FillLocation(file_location_cache, item);
      co_yield std::move(item);

    // Fields, i.e. instance members.
    } else if (auto fd = FieldDecl::from(decl)) {

      item.category = QObject::tr("Members");
      item.entity = fd.value();
      FillLocation(file_location_cache, item);

      // Make the field have `NNN.N` offsets as bit and byte offsets.
      if (auto offset = fd->offset_in_bits()) {

        std::stringstream ss;
        ss << std::setw(num_digits) << std::setfill(' ')
           << (offset.value() / 8u);

        if (has_bits) {
          ss << '.' << (offset.value() % 8u);
        }

        tok.category = TokenCategory::LITERAL;
        tok.kind = TokenKind::NUMERIC_CONSTANT;
        tok.data = ss.str();
        toks.emplace_back(std::move(tok));

        tok.category = TokenCategory::WHITESPACE;
        tok.kind = TokenKind::WHITESPACE;
        tok.data = " ";
        toks.emplace_back(std::move(tok));

        for (Token name_tok : NameOfEntity(decl, false  /* qualify */)) {
          toks.emplace_back(std::move(name_tok));
        }

        item.tokens = TokenRange::create(std::move(toks));

      } else {
        item.tokens = NameOfEntity(decl, false  /* qualify */);
      }

      co_yield std::move(item);

    } else if (auto md = CXXMethodDecl::from(decl)) {
      switch (decl.kind()) {
        case DeclKind::CXX_CONSTRUCTOR:
          item.category = QObject::tr("Constructors");
          break;
        case DeclKind::CXX_CONVERSION:
          item.category = QObject::tr("Conversion Operators");
          break;
        case DeclKind::CXX_DEDUCTION_GUIDE:
          item.category = QObject::tr("Deduction Guides");
          break;
        case DeclKind::CXX_DESTRUCTOR:
          item.category = QObject::tr("Destructors");
          break;
        default:
          if (md->overloaded_operator() != OverloadedOperatorKind::NONE) {
            item.category = QObject::tr("Overloaded Operators");
          } else if (md->is_instance()) {
            item.category = QObject::tr("Instance Methods");
          } else {
            item.category = QObject::tr("Class Methods");
          }
          break;
      }

      item.tokens = md->token();
      item.entity = std::move(md.value());
      FillLocation(file_location_cache, item);
      co_yield std::move(item);

    } else if (auto td = TagDecl::from(decl)) {
      (void) td;
    }

    // TODO(pag): Friend classes, functions
  }
}

// Generate information about files. This primarily focuses on top-level
// entities in the file.
template <>
gap::generator<IInfoGenerator::Item> EntityInfoGenerator<File>::Items(
    IInfoGeneratorPtr, FileLocationCache file_location_cache) {
  
  std::vector<CustomToken> toks;
  UserToken tok;
  IInfoGenerator::Item item;

  for (IncludeLikeMacroDirective inc : IncludeLikeMacroDirective::in(entity)) {
    if (std::optional<File> file = inc.included_file()) {
      item.category = QObject::tr("Includes");
      item.tokens = inc.use_tokens().strip_whitespace();
      item.entity = std::move(inc);
      FillLocation(file_location_cache, item);
      co_yield std::move(item);
    }
  }

  for (Reference ref : Reference::to(entity)) {
    auto inc = IncludeLikeMacroDirective::from(ref.as_macro());
    if (!inc) {
      continue;
    }

    // Find the file containing the `#include`, then build up a bunch of
    // `TokenRange`s that contain `file:line:column` triples for the location
    // of the actual `#include` itself.
    auto file = File::containing(inc.value());
    if (!file) {
      continue;
    }

    auto loc = inc->use_tokens().front().location(file_location_cache);
    if (!loc) {
      continue;
    }

    item.category = QObject::tr("Included By");
    item.entity = std::move(inc.value());
    FillLocation(file_location_cache, item);

    tok.category = TokenCategory::FILE_NAME;
    tok.kind = TokenKind::HEADER_NAME;
    tok.related_entity = file.value();
    for (std::filesystem::path file_path : file->paths()) {
      tok.data = file_path.generic_string();
      break;
    }
    toks.emplace_back(std::move(tok));

    tok.category = TokenCategory::PUNCTUATION;
    tok.kind = TokenKind::COLON;
    tok.data = ":";
    toks.emplace_back(std::move(tok));

    tok.category = TokenCategory::LINE_NUMBER;
    tok.kind = TokenKind::NUMERIC_CONSTANT;
    tok.data = std::to_string(loc->first);
    toks.emplace_back(std::move(tok));

    tok.category = TokenCategory::PUNCTUATION;
    tok.kind = TokenKind::COLON;
    tok.data = ":";
    toks.emplace_back(std::move(tok));

    tok.category = TokenCategory::LINE_NUMBER;
    tok.kind = TokenKind::NUMERIC_CONSTANT;
    tok.data = std::to_string(loc->second);
    toks.emplace_back(std::move(tok));

    item.tokens = TokenRange::create(std::move(toks));
    co_yield std::move(item);
  }

  std::vector<Decl> work_list;

  // Find the top-level entities in this file.
  for (Fragment frag : entity.fragments()) {
    for (DefineMacroDirective def : DefineMacroDirective::in(frag)) {
      item.category = QObject::tr("Defined Macros");
      item.tokens = def.name();
      item.entity = std::move(def);
      FillLocation(file_location_cache, item);
      co_yield std::move(item);
    }

    work_list.clear();
    for (Decl decl : frag.top_level_declarations()) {
      work_list.emplace_back(std::move(decl));
    }

    for (auto i = 0ull; i < work_list.size(); ++i) {
      auto decl = std::move(work_list[i]);
      auto nd = NamedDecl::from(decl);
      if (!nd) {
        continue;
      }

      switch (Token::categorize(decl)) {
        case TokenCategory::ENUM:
          item.category = QObject::tr("Enums");
          break;
        case TokenCategory::ENUMERATOR:
          item.category = QObject::tr("Enumerators");
          break;
        case TokenCategory::CLASS:
          item.category = QObject::tr("Classes");
          break;
        case TokenCategory::STRUCT:
          item.category = QObject::tr("Structures");
          break;
        case TokenCategory::UNION:
          item.category = QObject::tr("Unions");
          break;
        case TokenCategory::CONCEPT:
          item.category = QObject::tr("Concepts");
          break;
        case TokenCategory::INTERFACE:
          item.category = QObject::tr("Interfaces");
          break;
        case TokenCategory::TYPE_ALIAS:
          item.category = QObject::tr("Types");
          break;
        case TokenCategory::FUNCTION:
        case TokenCategory::CLASS_METHOD:
          item.category = QObject::tr("Functions");
          break;
        case TokenCategory::LOCAL_VARIABLE:
          Q_ASSERT(false);  // Strange.
        case TokenCategory::GLOBAL_VARIABLE:
        case TokenCategory::CLASS_MEMBER:
          item.category = QObject::tr("Global Variables");
          break;
        default:
          item.category = QObject::tr("Top Level Entities");
          break;
      }

      // Descend into enums.
      if (auto ed = EnumDecl::from(decl)) {
        if (ed->is_definition()) {
          for (auto enumerator : ed->enumerators()) {
            work_list.emplace_back(std::move(enumerator));
          }
        }
      }

      // Descend into records.
      if (auto rd = RecordDecl::from(decl)) {
        if (rd->is_definition()) {
          for (auto nested_decl : rd->declarations_in_context()) {
            switch (Token::categorize(nested_decl)) {
              case TokenCategory::ENUM:
              case TokenCategory::CLASS_METHOD:
              case TokenCategory::GLOBAL_VARIABLE:
              case TokenCategory::CLASS:
              case TokenCategory::STRUCT:
              case TokenCategory::UNION:
              case TokenCategory::CONCEPT:
              case TokenCategory::INTERFACE:
              case TokenCategory::TYPE_ALIAS:
                work_list.emplace_back(std::move(nested_decl));
                break;
              default:
                break;
            }
          }
        }
      }

      item.entity = std::move(decl);
      FillLocation(file_location_cache, item);
      item.tokens = NameOfEntity(nd.value());
      co_yield std::move(item);
    }
  }
}

// Generate information about macros. This primarily focuses on expansions of
// defined macros.
template <>
gap::generator<IInfoGenerator::Item> EntityInfoGenerator<Macro>::Items(
    IInfoGeneratorPtr, FileLocationCache file_location_cache) {

  auto def = DefineMacroDirective::from(entity);
  if (!def) {
    co_return;
  }

  std::vector<CustomToken> toks;
  UserToken tok;
  IInfoGenerator::Item item;

  // Tell us where the macro is defined.
  item.category = QObject::tr("Definitions");
  item.tokens = def->name();
  item.entity = std::move(entity);
  FillLocation(file_location_cache, item);
  co_yield std::move(item);

  // Find the macro parameters.
  for (const MacroOrToken &mt : def->parameters()) {
    if (!std::holds_alternative<Macro>(mt)) {
      continue;
    }

    auto mp = MacroParameter::from(std::get<Macro>(mt));
    if (!mp) {
      continue;
    }

    TokenRange tokens = mp->use_tokens();
    if (Token name_tok = mp->name()) {
      if (def->is_variadic()) {
        item.tokens = std::move(tokens);
      } else {
        item.tokens = std::move(name_tok);
      }

    } else if (def->is_variadic()) {
      tok.category = TokenCategory::MACRO_PARAMETER_NAME;
      tok.kind = TokenKind::IDENTIFIER;
      tok.data = "__VA_ARGS__";
      tok.related_entity = mp.value();
      toks.emplace_back(std::move(tok));

      item.tokens = TokenRange::create(std::move(toks));
    }

    item.category = QObject::tr("Parameters");
    item.entity = std::move(mp.value());
    FillLocation(file_location_cache, item);
    co_yield std::move(item);
  }

  // Look for expansions of the macro.
  for (Reference ref : Reference::to(def.value())) {
    auto exp = MacroExpansion::from(ref.as_macro());
    if (!exp) {
      continue;
    }

    TokenRange tokens = exp->use_tokens();
    item.category = QObject::tr("Expansions");
    item.tokens = InjectWhitespace(tokens.strip_whitespace());
    item.entity = std::move(exp.value());
    FillLocation(file_location_cache, item);
    co_yield std::move(item);
  }
}

}  // namespace

BuiltinEntityInformationPlugin::~BuiltinEntityInformationPlugin(void) {}

gap::generator<IInfoGeneratorPtr>
BuiltinEntityInformationPlugin::CreateInformationCollectors(
    VariantEntity entity) {

  if (auto file = File::from(entity)) {
    co_yield std::make_shared<EntityInfoGenerator<File>>(
        std::move(file.value()));
    co_return;
  }

  if (auto macro = Macro::from(entity)) {
    co_yield std::make_shared<EntityInfoGenerator<Macro>>(
        std::move(macro.value()));
    co_return;
  }

  if (auto record = RecordDecl::from(entity)) {
    co_yield std::make_shared<EntityInfoGenerator<RecordDecl>>(
        std::move(record.value()));
  }
}

}  // namespace mx::gui
