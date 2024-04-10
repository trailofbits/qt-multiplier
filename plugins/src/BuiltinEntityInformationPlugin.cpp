// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Plugins/BuiltinEntityInformationPlugin.h>

#include <multiplier/AST/CallExpr.h>
#include <multiplier/AST/CastExpr.h>
#include <multiplier/AST/CXXMethodDecl.h>
#include <multiplier/AST/DeclKind.h>
#include <multiplier/AST/EnumDecl.h>
#include <multiplier/AST/EnumConstantDecl.h>
#include <multiplier/AST/FieldDecl.h>
#include <multiplier/AST/RecordDecl.h>
#include <multiplier/AST/TypeTraitExpr.h>
#include <multiplier/AST/UnaryExprOrTypeTraitExpr.h>
#include <multiplier/AST/VarDecl.h>
#include <multiplier/Frontend/MacroExpansion.h>
#include <multiplier/Frontend/MacroParameter.h>
#include <multiplier/GUI/Util.h>
#include <multiplier/Index.h>

#include <cstdlib>
#include <memory>
#include <sstream>

namespace mx::gui {
namespace {

// Builds a TokenRange for the given Location
static TokenRange
GetLocationFileName(const EntityLocation &location,
                    const VariantEntity &referenced_entity) {
  UserToken user_tok;
  user_tok.category = TokenCategory::FILE_NAME;
  user_tok.kind = TokenKind::HEADER_NAME;
  user_tok.related_entity = referenced_entity;
  user_tok.data = location.path.filename();

  std::vector<CustomToken> toks;
  toks.emplace_back(std::move(user_tok));

  user_tok.category = TokenCategory::PUNCTUATION;
  user_tok.kind = TokenKind::COLON;
  user_tok.data = ":";
  toks.emplace_back(std::move(user_tok));

  user_tok.category = TokenCategory::LINE_NUMBER;
  user_tok.kind = TokenKind::NUMERIC_CONSTANT;
  user_tok.data = std::to_string(location.line);
  toks.emplace_back(std::move(user_tok));

  user_tok.category = TokenCategory::PUNCTUATION;
  user_tok.kind = TokenKind::COLON;
  user_tok.data = ":";
  toks.emplace_back(std::move(user_tok));

  user_tok.category = TokenCategory::LINE_NUMBER;
  user_tok.kind = TokenKind::NUMERIC_CONSTANT;
  user_tok.data = std::to_string(location.column);
  toks.emplace_back(std::move(user_tok));

  return TokenRange::create(std::move(toks));
}

// Fill the location entry in an generated item.
static void FillLocation(const FileLocationCache &file_location_cache,
                         IInfoGenerator::Item &item,
                         const bool &skip_file_name_loc = false) {

  auto opt_location = LocationOfEntityEx(file_location_cache, item.entity);
  if (!opt_location.has_value()) {
    item.location = QObject::tr("Entity ID: %1").arg(EntityId(item.entity).Pack());
    item.file_name_location = std::nullopt;

  } else if (!skip_file_name_loc) {
    const auto &location = opt_location.value();

    item.location = QString("%1:%2:%3")
                      .arg(QString::fromStdString(location.path.generic_string()))
                      .arg(location.line)
                      .arg(location.column);

    item.file_name_location = GetLocationFileName(location, item.referenced_entity);
  }
}

// Generates information about `T`s.
template <typename T>
class EntityInfoGenerator Q_DECL_FINAL : public IInfoGenerator {
 public:
  T entity;

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
      item.referenced_entity = item.entity;
      FillLocation(file_location_cache, item);
      co_yield std::move(item);

    // Fields, i.e. instance members.
    } else if (auto fd = FieldDecl::from(decl)) {

      item.category = QObject::tr("Members");
      item.entity = fd.value();
      item.referenced_entity = item.entity;
      item.tokens = TokenRange();
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

      item.tokens = md->token();

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
        case DeclKind::CXX_DESTRUCTOR: {
          item.category = QObject::tr("Destructors");
          auto full_name = md->name();

          tok.related_entity = decl;
          tok.category = Token::categorize(tok.related_entity);
          tok.kind = TokenKind::IDENTIFIER;
          tok.data.insert(tok.data.end(), full_name.begin(), full_name.end());
          toks.emplace_back(std::move(tok));
          item.tokens = TokenRange::create(std::move(toks)).front();
          break;
        }
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

      item.entity = std::move(md.value());
      item.referenced_entity = item.entity;
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
      item.referenced_entity = NotAnEntity{};
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
    item.referenced_entity = file.value();
    item.tokens = TokenRange();
    FillLocation(file_location_cache, item, true);

    tok.category = TokenCategory::FILE_NAME;
    tok.kind = TokenKind::HEADER_NAME;
    tok.related_entity = item.referenced_entity;
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
      item.referenced_entity = item.entity;
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
      item.referenced_entity = item.entity;
      item.tokens = TokenRange();
      FillLocation(file_location_cache, item);
      item.tokens = NameOfEntity(nd.value());
      co_yield std::move(item);
    }
  }
}

// Generate information about macro definitions. This primarily focuses on
// expansions of those macros.
template <>
gap::generator<IInfoGenerator::Item>
EntityInfoGenerator<DefineMacroDirective>::Items(
    IInfoGeneratorPtr, FileLocationCache file_location_cache) {

  std::vector<CustomToken> toks;
  UserToken tok;
  IInfoGenerator::Item item;

  // Tell us where the macro is defined.
  item.category = QObject::tr("Definitions");
  item.tokens = entity.name();
  item.entity = entity;
  item.referenced_entity = item.entity;
  FillLocation(file_location_cache, item);
  co_yield std::move(item);

  // Find the macro parameters.
  for (const MacroOrToken &mt : entity.parameters()) {
    if (!std::holds_alternative<Macro>(mt)) {
      continue;
    }

    auto mp = MacroParameter::from(std::get<Macro>(mt));
    if (!mp) {
      continue;
    }

    TokenRange tokens = mp->use_tokens();
    if (Token name_tok = mp->name()) {
      if (entity.is_variadic()) {
        item.tokens = std::move(tokens);
      } else {
        item.tokens = std::move(name_tok);
      }

    } else if (entity.is_variadic()) {
      tok.category = TokenCategory::MACRO_PARAMETER_NAME;
      tok.kind = TokenKind::IDENTIFIER;
      tok.data = "__VA_ARGS__";
      tok.related_entity = mp.value();
      toks.emplace_back(std::move(tok));

      item.tokens = TokenRange::create(std::move(toks));
    }

    item.category = QObject::tr("Parameters");
    item.entity = std::move(mp.value());
    item.referenced_entity = NotAnEntity{};
    FillLocation(file_location_cache, item);
    co_yield std::move(item);
  }

  // Look for expansions of the macro.
  for (Reference ref : Reference::to(entity)) {
    auto exp = MacroExpansion::from(ref.as_macro());
    if (!exp) {
      continue;
    }

    item.category = QObject::tr("Expansions");
    item.tokens = InjectWhitespace(exp->use_tokens().strip_whitespace());
    item.entity = std::move(exp.value());
    item.referenced_entity = NotAnEntity{};
    FillLocation(file_location_cache, item);
    co_yield std::move(item);
  }
}

// Generate information about type decls. This focuses on their size and
// alignment, and uses.
template <>
gap::generator<IInfoGenerator::Item> EntityInfoGenerator<TypeDecl>::Items(
    IInfoGeneratorPtr, FileLocationCache file_location_cache) {
  auto type = entity.type_for_declaration();
  if (!type) {
    co_return;
  }

  IInfoGenerator::Item item;

  if (auto size = type->size_in_bits()) {
    item.category = QObject::tr("Size");
    if (!(size.value() % 8u)) {
      item.location = QObject::tr("Size %1 (bytes)").arg(size.value() / 8u);
    } else {
      item.location = QObject::tr("Size %1 (bits)").arg(size.value());
    }
    co_yield std::move(item);
  }

  if (auto align = type->alignment()) {
    item.category = QObject::tr("Size");
    item.location = QObject::tr("Alignment %1 (bytes)").arg(align.value());
    co_yield std::move(item);
  }

  for (Reference ref : Reference::to(entity)) {
    if (!ref.builtin_reference_kind()) {
      continue;
    }

    auto context = ref.context();

    if (auto du = Decl::from(context)) {
      item.category = QObject::tr("Declaration Uses");
      item.entity = std::move(context);
      item.referenced_entity = item.entity;
      item.tokens = TokenRange();
      FillLocation(file_location_cache, item);

      if (FunctionDecl::from(du.value())) {
        item.tokens = NameOfEntity(du.value());
      } else {
        item.tokens = InjectWhitespace(du->tokens().strip_whitespace());
      }

      co_yield std::move(item);

    } else if (auto ce = CastExpr::from(context)) {
      item.category = QObject::tr("Type Casts");
      item.entity = std::move(context);
      item.referenced_entity = NotAnEntity{};
      item.tokens = TokenRange();
      FillLocation(file_location_cache, item);

      item.tokens = InjectWhitespace(ce->tokens().strip_whitespace());
      co_yield std::move(item);

    } else if (auto tte = TypeTraitExpr::from(context)) {
      item.category = QObject::tr("Trait Uses");
      item.entity = std::move(context);
      item.referenced_entity = NotAnEntity{};
      item.tokens = TokenRange();
      FillLocation(file_location_cache, item);
      
      item.tokens = InjectWhitespace(tte->tokens().strip_whitespace());
      co_yield std::move(item);

    } else if (auto uett = UnaryExprOrTypeTraitExpr::from(context)) {
      switch (uett->keyword_kind()) {
        case UnaryExprOrTypeTrait::SIZE_OF:
          item.category = QObject::tr("Size Ofs");
          break;
        case UnaryExprOrTypeTrait::ALIGN_OF:
        case UnaryExprOrTypeTrait::PREFERRED_ALIGN_OF:
          item.category = QObject::tr("Align Ofs");
          break;
        case UnaryExprOrTypeTrait::POINTER_AUTH_TYPE_DISCRIMINATOR:
        case UnaryExprOrTypeTrait::XNU_TYPE_SIGNATURE:
        case UnaryExprOrTypeTrait::XNU_TYPE_SUMMARY:
        case UnaryExprOrTypeTrait::TMO_TYPE_GET_METADATA:
          item.category = QObject::tr("Security Type Traits");
          break;
        case UnaryExprOrTypeTrait::VEC_STEP:
        case UnaryExprOrTypeTrait::OPEN_MP_REQUIRED_SIMD_ALIGN:
          item.category = QObject::tr("Vector Type Traits");
          break;
      }
      item.entity = std::move(context);
      item.referenced_entity = NotAnEntity{};
      item.tokens = TokenRange();
      FillLocation(file_location_cache, item);
      
      item.tokens = InjectWhitespace(uett->tokens().strip_whitespace());
      co_yield std::move(item);

    } else if (auto s = Stmt::from(context)) {
      item.category = QObject::tr("Statement Uses");
      item.entity = std::move(context);
      item.referenced_entity = NotAnEntity{};
      item.tokens = TokenRange();
      FillLocation(file_location_cache, item);
      
      item.tokens = InjectWhitespace(s->tokens().strip_whitespace());
      co_yield std::move(item);
    }
  }
}

// Generate information about enums. This focuses on their enumerators.
template <>
gap::generator<IInfoGenerator::Item> EntityInfoGenerator<EnumDecl>::Items(
    IInfoGeneratorPtr, FileLocationCache file_location_cache) {

  IInfoGenerator::Item item;

  for (mx::EnumConstantDecl ec : entity.canonical_declaration().enumerators()) {
    item.category = QObject::tr("Enumerators");
    item.tokens = ec.token();
    item.entity = std::move(ec);
    item.referenced_entity = item.entity;
    FillLocation(file_location_cache, item);
    co_yield std::move(item);
  }
}

// Generate information about functions. This focuses on their enumerators.
template <>
gap::generator<IInfoGenerator::Item> EntityInfoGenerator<FunctionDecl>::Items(
    IInfoGeneratorPtr, FileLocationCache file_location_cache) {

  IInfoGenerator::Item item;

  for (Reference ref : Reference::to(entity)) {
    auto brk = ref.builtin_reference_kind();
    if (!brk) {
      continue;
    }

    switch (brk.value()) {
      case BuiltinReferenceKind::CALLS:
        item.category = QObject::tr("Called By");
        break;
      case BuiltinReferenceKind::TAKES_ADDRESS:
        item.category = QObject::tr("Address Ofs");
        break;
      default:
        item.category = QObject::tr("Users");
        break;
    }

    item.entity = ref.context();
    if (std::holds_alternative<NotAnEntity>(item.entity)) {
      item.entity = ref.as_variant();
    }

    item.tokens = InjectWhitespace(Tokens(item.entity));
    FillLocation(file_location_cache, item);
    co_yield std::move(item);
  }

  // Find the callees. Slightly annoying as we kind of have to invent a join.
  //
  // TODO(pag): Make `::in(entity)` work for all entities, not just files
  //            and fragments.
  Fragment frag = Fragment::containing(entity);
  for (CallExpr call : CallExpr::in(frag)) {

    for (Reference ref : Reference::from(call)) {
      auto brk = ref.builtin_reference_kind();
      if (!brk || brk.value() != BuiltinReferenceKind::CALLS) {
        continue;
      }

      auto callee = FunctionDecl::from(ref.as_variant());
      if (!callee) {
        continue;
      }

      item.category = QObject::tr("Callees");
      item.entity = std::move(callee.value());
      item.referenced_entity = item.entity;
      item.tokens = TokenRange();
      FillLocation(file_location_cache, item);
      item.tokens = NameOfEntity(item.entity);
      item.entity = call;  // Yes, overwrite.
      co_yield std::move(item);
    }
  }

  // Find the local variables.
  for (const mx::Decl &decl : entity.declarations_in_context()) {
    std::optional<VarDecl> vd = mx::VarDecl::from(decl);
    if (!vd) {
      continue;
    }

    if (vd->kind() == DeclKind::PARM_VAR) {
      item.category = QObject::tr("Parameters");
    } else {
      if (vd->tsc_spec() != ThreadStorageClassSpecifier::UNSPECIFIED) {
        item.category = QObject::tr("Thread Local Variables");

      } else if (vd->storage_duration() == StorageDuration::STATIC) {
        item.category = QObject::tr("Static Local Variables");

      } else {
        item.category = QObject::tr("Local Variables");
      }
    }
    item.tokens = NameOfEntity(decl, false  /* don't fully qualify name */);
    item.entity = std::move(vd.value());
    item.referenced_entity = item.entity;
    FillLocation(file_location_cache, item);
    co_yield std::move(item);
  }
}

// Generate information about variables.
template <>
gap::generator<IInfoGenerator::Item> EntityInfoGenerator<ValueDecl>::Items(
    IInfoGeneratorPtr, FileLocationCache file_location_cache) {

  IInfoGenerator::Item item;

  auto type = entity.type();
  item.category = QObject::tr("Type");
  item.tokens = InjectWhitespace(type.tokens());
  FillLocation(file_location_cache, item);
  co_yield std::move(item);

  for (Reference ref : Reference::to(entity)) {
    auto brk = ref.builtin_reference_kind();
    if (!brk) {
      continue;
    }

    switch (brk.value()) {
      case BuiltinReferenceKind::CASTS_WITH_TYPE:
        item.category = QObject::tr("Casted By");
        break;
      case BuiltinReferenceKind::COPIES_VALUE:
        item.category = QObject::tr("Copied Into");
        break;
      case BuiltinReferenceKind::TESTS_VALUE:
        item.category = QObject::tr("Tested By");
        break;
      case BuiltinReferenceKind::WRITES_VALUE:
        item.category = QObject::tr("Written By");
        break;
      case BuiltinReferenceKind::UPDATES_VALUE:
        item.category = QObject::tr("Updated By");
        break;
      case BuiltinReferenceKind::ACCESSES_VALUE:
        item.category = QObject::tr("Dereferenced By");
        break;
      case BuiltinReferenceKind::TAKES_VALUE:
        item.category = QObject::tr("Passed As Argument To");
        break;
      case BuiltinReferenceKind::CALLS:
        item.category = QObject::tr("Called By");
        break;
      case BuiltinReferenceKind::TAKES_ADDRESS:
        item.category = QObject::tr("Address Taken By");
        break;
      case BuiltinReferenceKind::USES_VALUE:
      default:
        item.category = QObject::tr("Used By");
        break;
    }

    item.entity = ref.context();
    if (std::holds_alternative<NotAnEntity>(item.entity)) {
      item.entity = ref.as_variant();
    }

    if (auto stmt = Stmt::from(item.entity)) {
      item.tokens = InjectWhitespace(stmt->tokens().strip_whitespace());
    } else {
      item.tokens = Tokens(item.entity);
    }

    FillLocation(file_location_cache, item);
    co_yield std::move(item);
  }
}

// Generate information about named declarations.
template <>
gap::generator<IInfoGenerator::Item> EntityInfoGenerator<NamedDecl>::Items(
    IInfoGeneratorPtr, FileLocationCache file_location_cache) {

  std::vector<PackedMacroId> seen;
  IInfoGenerator::Item item;

  entity = entity.canonical_declaration();

  // Fill all redeclarations.
  for (NamedDecl redecl : entity.redeclarations()) {
    if (redecl.is_definition()) {
      item.category = QObject::tr("Definitions");
    } else {
      item.category = QObject::tr("Declarations");
    }

    item.entity = redecl;
    item.referenced_entity = item.entity;
    item.tokens = TokenRange();
    FillLocation(file_location_cache, item);

    item.tokens = NameOfEntity(item.entity, true  /* qualify */,
                               false  /* don't scan redecls */);
    co_yield std::move(item);

    for (Token tok : redecl.tokens()) {
      for (Macro macro : Macro::containing(tok)) {
        macro = macro.root();
        if (macro.kind() != MacroKind::EXPANSION) {
          break;
        }

        PackedMacroId macro_id = macro.id();
        if (std::find(seen.begin(), seen.end(), macro_id) != seen.end()) {
          break;
        }

        seen.push_back(macro_id);
        std::optional<MacroExpansion> exp = MacroExpansion::from(macro);
        if (!exp) {
          break;
        }

        auto def = exp->definition();
        if (!def) {
          break;
        }

        auto tokens = InjectWhitespace(exp->use_tokens().strip_whitespace());
        item.category = QObject::tr("Macros Used");
        item.entity = std::move(exp.value());
        item.referenced_entity = def.value();
        item.tokens = TokenRange();
        FillLocation(file_location_cache, item);
        item.tokens = std::move(tokens);
        co_yield std::move(item);
        break;
      }
    }
  }

  if (entity.parent_declaration()) {
    for (std::optional<Decl> pd = entity; pd; pd = pd->parent_declaration()) {
      item.category = QObject::tr("Parentage");
      item.entity = pd.value();
      item.referenced_entity = item.entity;
      item.tokens = TokenRange();
      FillLocation(file_location_cache, item);
      item.tokens = NameOfEntity(item.entity, false  /* don't qualify */);
      co_yield std::move(item);
    }
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

  if (auto dmd = DefineMacroDirective::from(entity)) {
    co_yield std::make_shared<EntityInfoGenerator<DefineMacroDirective>>(
        std::move(dmd.value()));
    co_return;
  }

  if (auto td = TypeDecl::from(entity)) {
    co_yield std::make_shared<EntityInfoGenerator<TypeDecl>>(
        std::move(td.value()));
  }

  if (auto rd = RecordDecl::from(entity)) {
    co_yield std::make_shared<EntityInfoGenerator<RecordDecl>>(
        std::move(rd.value()));
  }

  if (auto ed = EnumDecl::from(entity)) {
    co_yield std::make_shared<EntityInfoGenerator<EnumDecl>>(
        std::move(ed.value()));
  }

  if (auto fd = FunctionDecl::from(entity)) {
    co_yield std::make_shared<EntityInfoGenerator<FunctionDecl>>(
        std::move(fd.value()));
  }

  if (auto xd = ValueDecl::from(entity)) {
    co_yield std::make_shared<EntityInfoGenerator<ValueDecl>>(
        std::move(xd.value()));
  }

  if (auto nd = NamedDecl::from(entity)) {
    co_yield std::make_shared<EntityInfoGenerator<NamedDecl>>(
        std::move(nd.value()));
  }
}

}  // namespace mx::gui
