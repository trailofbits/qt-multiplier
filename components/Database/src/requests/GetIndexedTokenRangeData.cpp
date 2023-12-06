/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "GetIndexedTokenRangeData.h"

#include <QChar>
#include <QDebug>
#include <QString>

#include <unordered_map>
#include <iostream>

#include <multiplier/AST/Stmt.h>
#include <multiplier/Frontend/Token.h>
#include <multiplier/Frontend/TokenTree.h>
#include <multiplier/ui/Assert.h>
#include <multiplier/ui/Util.h>

namespace mx::gui {
namespace {

// Render `tok` across one or more lines of `res`.
static void RenderToken(const FileLocationCache &file_location_cache,
                        IndexedTokenRangeData &res, Token tok,
                        unsigned tok_index) {

  IndexedTokenRangeData::Column dummy;
  IndexedTokenRangeData::Column *split_self = &dummy;
  IndexedTokenRangeData::Line *line = &(res.lines.back());

  // Try to get a number for this line.
  unsigned line_number_from_tok = 0u;
  VariantId token_vid = tok.id().Unpack();
  if (std::holds_alternative<FileTokenId>(token_vid)) {
    FileId fid(std::get<FileTokenId>(token_vid).file_id);
    if (EntityId(fid) == res.file_id) {
      if (auto line_col = tok.location(file_location_cache)) {
        line_number_from_tok = line_col->first;
        if (!line->number) {
          line->number = line_col->first;
        }
      }
    }
  }

  // Get the data of this token in Qt's native format.
  std::string_view utf8_data = tok.data();
  QString utf16_data;
  if (!utf8_data.empty()) {
    utf16_data = QString::fromUtf8(utf8_data.data(),
                                   static_cast<qsizetype>(utf8_data.size()));
  }

  // Split the token for lines and such.
  IndexedTokenRangeData::Column col;
  col.category = tok.category();
  col.token_index = tok_index;

  for (QChar ch : utf16_data) {
    switch (ch.unicode()) {
      case QChar::Tabulation: col.data.append(QChar::Tabulation); break;

      case QChar::Space:
      case QChar::Nbsp: col.data.append(QChar::Space); break;

      case QChar::ParagraphSeparator:
      case QChar::LineFeed:
      case QChar::LineSeparator: {
        if (!col.data.isEmpty()) {
          split_self->split_across_lines = true;
          split_self = &(line->columns.emplace_back(col));
        }

        // Reset.
        col.data = QString();
        col.starts_on_line = false;
        col.split_across_lines = true;

        // Start the next line.
        line = &(res.lines.emplace_back());

        // If this token contributed its line number, and if this token spans
        // more than one line, then we can use this token's starting line
        // number to set the subsequent line numbers.
        if (line_number_from_tok) {
          line->number = ++line_number_from_tok;
        }

        break;
      }

      case QChar::CarriageReturn: continue;

      // TODO(pag): Consult with QFontMetrics or something else to determine
      //            if this character is visible?
      default: col.data.append(ch); break;
    }
  }

  if (!col.data.isEmpty()) {
    split_self->split_across_lines = true;
    line->columns.emplace_back(std::move(col));
  }
}

// Figure out if `tok` is visible in the file.
static bool IsTopLevelToken(const Token &tok) {
  VariantId vid = tok.id().Unpack();
  if (std::holds_alternative<FileTokenId>(vid)) {
    return true;
  }

  for (Macro m : Macro::containing(tok)) {
    switch (m.kind()) {
      case MacroKind::ARGUMENT: continue;

      // Directive tokens are always top level.
      case MacroKind::PARAMETER:
      case MacroKind::OTHER_DIRECTIVE:
      case MacroKind::IF_DIRECTIVE:
      case MacroKind::IF_DEFINED_DIRECTIVE:
      case MacroKind::IF_NOT_DEFINED_DIRECTIVE:
      case MacroKind::ELSE_IF_DIRECTIVE:
      case MacroKind::ELSE_IF_DEFINED_DIRECTIVE:
      case MacroKind::ELSE_IF_NOT_DEFINED_DIRECTIVE:
      case MacroKind::ELSE_DIRECTIVE:
      case MacroKind::END_IF_DIRECTIVE:
      case MacroKind::DEFINE_DIRECTIVE:
      case MacroKind::UNDEFINE_DIRECTIVE:
      case MacroKind::PRAGMA_DIRECTIVE:
      case MacroKind::INCLUDE_DIRECTIVE:
      case MacroKind::INCLUDE_NEXT_DIRECTIVE:
      case MacroKind::INCLUDE_MACROS_DIRECTIVE:
      case MacroKind::IMPORT_DIRECTIVE: return true;

      // These all exist in the `MacroExpansion::intermediate_children` or
      // `MacroExpansion::replacement_children`
      case MacroKind::PARAMETER_SUBSTITUTION:
      case MacroKind::STRINGIFY:
      case MacroKind::CONCATENATE:
      case MacroKind::VA_OPT:
      case MacroKind::VA_OPT_ARGUMENT: return false;

      // Check if `tok` is in the use.
      case MacroKind::SUBSTITUTION:
      case MacroKind::EXPANSION: {
        auto found = false;
        for (MacroOrToken mt : m.children()) {
          if (std::holds_alternative<Token>(mt) && std::get<Token>(mt) == tok) {
            found = true;
            break;
          }
        }
        if (!found) {
          return false;
        }
        continue;
      }
    }
  }

  return true;
}

static void GenerateLineHashes(IndexedTokenRangeData &res) {
  auto xxhash_state = XXH64_createState();
  Assert(xxhash_state != nullptr, "Failed to initialize the xxHash state");

  for (IndexedTokenRangeData::Line &line : res.lines) {
    static const XXH64_hash_t kXxhashSeed{0};

    auto succeeded = XXH64_reset(xxhash_state, kXxhashSeed) != XXH_ERROR;
    Assert(succeeded, "Failed to reset the xxHash state");

    Assert(succeeded, "Failed to update the xxHash state");

    for (const auto &column : line.columns) {
      const auto &token = res.tokens[column.token_index];

      auto token_id = token.id();
      succeeded =
          XXH64_update(xxhash_state, &token_id, sizeof(token_id)) != XXH_ERROR;

      Assert(succeeded, "Failed to update the xxHash state");

      const auto &token_str = column.data;
      succeeded = XXH64_update(xxhash_state, token_str.data(),
                               token_str.size()) != XXH_ERROR;

      Assert(succeeded, "Failed to update the xxHash state");

      line.hash = XXH64_digest(xxhash_state);
    }
  }

  XXH64_freeState(xxhash_state);
}

// Fixup the line numbers from the visible tokens.
static void FixupLineNumbers(const FileLocationCache &file_location_cache,
                             IndexedTokenRangeData &res) {
  for (IndexedTokenRangeData::Line &line : res.lines) {
    if (line.number) {
      continue;
    }

    auto num_cols = line.columns.size();
    if (!num_cols) {
      continue;
    }

    if (!line.columns.front().starts_on_line) {
      continue;
    }

    for (IndexedTokenRangeData::Column &c : line.columns) {
      if (!c.starts_on_line) {
        break;
      }

      Token tok = res.tokens[c.token_index];
      if (IsTopLevelToken(tok)) {
        if (auto line_col = tok.location(file_location_cache)) {
          line.number = line_col->first;
          goto next;
        }
      }
    }

  next:
    continue;
  }
}

static void PostProcessLineObjects(const FileLocationCache &file_location_cache,
                                   IndexedTokenRangeData &res) {
  FixupLineNumbers(file_location_cache, res);
  GenerateLineHashes(res);
}

void GetExpandedTokenRangeDataImpl(
    QPromise<IDatabase::IndexedTokenRangeDataResult> &result_promise,
    const Index &, const FileLocationCache &file_location_cache,
    RawEntityId entity_id, TokenTree tree, const TokenTreeVisitor *vis,
    std::optional<std::pair<unsigned, unsigned>> line_col) {

  IndexedTokenRangeData res;
  std::optional<Fragment> frag = Fragment::containing(tree);
  std::optional<File> file = File::containing(tree);

  if (frag) {
    res.response_id = frag->id().Pack();

  } else if (file) {
    res.response_id = file->id().Pack();
  }

  if (file) {
    res.file_id = file->id().Pack();
  }

  res.requested_id = entity_id;
  res.line_col = std::move(line_col);
  res.tokens = tree.serialize(*vis);
  res.lines.emplace_back();

  unsigned tok_index = 0u;
  for (Token tok : res.tokens) {
    RenderToken(file_location_cache, res, std::move(tok), tok_index++);
  }

  if (res.lines.back().columns.empty()) {
    res.lines.pop_back();
  }

  PostProcessLineObjects(file_location_cache, res);

  result_promise.addResult(std::move(res));
}

}  // namespace

void GetExpandedTokenRangeData(
    QPromise<IDatabase::IndexedTokenRangeDataResult> &result_promise,
    const Index &index, const FileLocationCache &file_location_cache,
    RawEntityId entity_id, TokenTree tree, const TokenTreeVisitor *vis) {

  VariantEntity ent = index.entity(entity_id);
  if (std::holds_alternative<NotAnEntity>(ent)) {
    result_promise.addResult(RPCErrorCode::InvalidEntityID);
    return;
  }

  TokenTreeVisitor empty_vis;
  if (!vis) {
    vis = &empty_vis;
  }

  GetExpandedTokenRangeDataImpl(
      result_promise, index, file_location_cache, entity_id, std::move(tree),
      vis, FirstFileToken(ent).location(file_location_cache));
}

void GetIndexedTokenRangeData(
    QPromise<IDatabase::IndexedTokenRangeDataResult> &result_promise,
    const Index &index, const FileLocationCache &file_location_cache,
    RawEntityId entity_id, const TokenTreeVisitor *vis) {

  VariantEntity ent = index.entity(entity_id);
  if (std::holds_alternative<NotAnEntity>(ent)) {
    result_promise.addResult(RPCErrorCode::InvalidEntityID);
    return;
  }

  TokenTree tree;
  if (std::holds_alternative<File>(ent)) {
    tree = TokenTree::from(std::get<File>(ent));

  } else if (std::holds_alternative<Fragment>(ent)) {
    tree = TokenTree::from(std::get<Fragment>(ent));

  } else if (auto frag = Fragment::containing(ent)) {
    tree = TokenTree::from(frag.value());

  } else if (auto file = File::containing(ent)) {
    tree = TokenTree::from(file.value());

  } else {
    // TODO(pag): Support token trees for types? That would go in mx-api.
    result_promise.addResult(RPCErrorCode::UnsupportedTokenTreeEntityType);
    return;
  }

  GetExpandedTokenRangeData(result_promise, index, file_location_cache,
                            entity_id, std::move(tree), vis);
}

}  // namespace mx::gui
