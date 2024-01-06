// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Plugins/BuiltinEntityInformationPlugin.h>

#include <multiplier/AST/CXXMethodDecl.h>
#include <multiplier/AST/DeclKind.h>
#include <multiplier/AST/FieldDecl.h>
#include <multiplier/AST/RecordDecl.h>
#include <multiplier/AST/VarDecl.h>
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

// Generates information about `RecordDecl`s.
class RecordInfoGenerator Q_DECL_FINAL : public IInfoGenerator {
 public:
  const RecordDecl entity;

  virtual ~RecordInfoGenerator(void) = default;

  inline RecordInfoGenerator(RecordDecl entity_)
      : entity(std::move(entity_)) {}

  gap::generator<IInfoGenerator::Item> Items(
      IInfoGeneratorPtr, FileLocationCache file_location_cache) Q_DECL_FINAL;
};

gap::generator<IInfoGenerator::Item> RecordInfoGenerator::Items(
    IInfoGeneratorPtr, FileLocationCache file_location_cache) {

  qDebug() << "RecordInfoGenerator::Items";

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

}  // namespace

BuiltinEntityInformationPlugin::~BuiltinEntityInformationPlugin(void) {}

gap::generator<IInfoGeneratorPtr>
BuiltinEntityInformationPlugin::CreateInformationCollectors(
    VariantEntity entity) {

  if (auto record = RecordDecl::from(entity)) {
    qDebug() << "Making a RecordInfoGenerator";
    co_yield std::make_shared<RecordInfoGenerator>(std::move(record.value()));
  }
}

}  // namespace mx::gui
