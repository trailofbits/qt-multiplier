/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "GetIndexedTokenRangeData.h"

#include <QChar>
#include <QString>

#include <unordered_map>

#include <multiplier/Entities/Stmt.h>
#include <multiplier/Entities/Token.h>
#include <multiplier/ui/Assert.h>

namespace mx::gui {
namespace {

// Render `tok` across one or more lines of `res`.
static void RenderToken(const FileLocationCache &file_location_cache,
                        IndexedTokenRangeData &res, Token tok,
                        unsigned tok_index) {

  IndexedTokenRangeData::Column dummy;
  IndexedTokenRangeData::Column *split_self = &dummy;
  IndexedTokenRangeData::Line *line = &(res.lines.back());

  VariantId vid = tok.id().Unpack();

  // Try to get a number for this line.
  unsigned prev_line_number = line->number;
  unsigned line_number_from_tok = 0u;
  if (std::holds_alternative<FileTokenId>(vid)) {
    if (auto line_col = tok.location(file_location_cache)) {
      Assert(!prev_line_number || prev_line_number == line_col->first,
             "Line number doesn't match expectations");
      line_number_from_tok = line_col->first;
      line->number = line_col->first;
    }
  }

  bool is_empty = true;

  // Get the data of this token in Qt's native format.
  std::string_view utf8_data = tok.data();
  QString utf16_data;
  if (!utf8_data.empty()) {
    utf16_data = QString::fromUtf8(
        utf8_data.data(), static_cast<qsizetype>(utf8_data.size()));
  }

  // Split the token for lines and such.
  IndexedTokenRangeData::Column col;
  col.category = tok.category();
  col.index = tok_index;
  col.offset = line->size;

  for (QChar ch : utf16_data) {
    switch (ch.unicode()) {
      case QChar::Tabulation:
        is_empty = false;
        ++res.size;
        ++line->size;
        col.data.append(QChar::Tabulation);
        break;

      case QChar::Space:
      case QChar::Nbsp:
        is_empty = false;
        ++res.size;
        ++line->size;
        col.data.append(QChar::Space);
        break;

      case QChar::ParagraphSeparator:
      case QChar::LineFeed:
      case QChar::LineSeparator: {
        ++res.size;
        ++line->size;

        col.data.append(QChar::LineSeparator);
        split_self->split_across_lines = true;
        split_self = &(line->columns.emplace_back(col));

        // Reset.
        col.data = QString();
        col.starts_on_line = false;
        col.split_across_lines = true;
        is_empty = true;

        // Start the next line.
        line = &(res.lines.emplace_back());
        line->offset = res.size;

        // If this token contributed its line number, and if this token spans
        // more than one line, then we can use this token's starting line
        // number to set the subsequent line numbers.
        if (line_number_from_tok) {
          line->number = ++line_number_from_tok;
        }

        break;
      }

      case QChar::CarriageReturn:
        continue;

      // TODO(pag): Consult with QFontMetrics or something else to determine
      //            if this character is visible?
      default:
        ++res.size;
        ++line->size;
        col.data.append(ch);
        is_empty = false;
        break;
    }
  }

  if (!is_empty) {
    split_self->split_across_lines = true;
    line->columns.emplace_back(std::move(col));
  }
}

//unsigned LowerBoundOfLine(const FileLocationCache &file_location_cache,
//                          const Token &tok) {
//  for (Macro m : Macro::containing(tok)) {
//    for (Token use_tok : m.root().generate_use_tokens()) {
//      if (auto file_line = use_tok.file_token().location(file_location_cache)) {
//        return file_line->first;
//      }
//    }
//  }
//
//  return 0u;
//}
//
//static void FixupLineNumbers(IndexedTokenRangeData &res,
//                             const FileLocationCache &file_location_cache) {
//  // Algorithm:
//  //  - get fragment containing a token, use it to have min/max line numbers
//  //    to bound things
//  //  - use lower bounds on the line numbers if contiguous with previous line
//  //    numbers, or if for first line
//  //  - use tok.file_token().location(...) otherwise, in in range.
//
//  unsigned last_line_number = 0u;
//
//  std::vector<unsigned> line_lbs;
//
//  for (IndexedTokenRangeData::Line &line : res.lines) {
//    if (line.number) {
//      last_line_number = line.number;
//      continue;
//    }
//
//    Assert(!line.starts_on_line.empty(), "Can't have completely empty lines");
//
//    unsigned tok_offset = 0u;
//    for (auto starts_on_line : line.starts_on_line) {
//      unsigned tok_index = line.token_index[tok_offset++];
//      if (starts_on_line) {
//        Token tok = res.tokens[tok_index];
//        if (auto lb_line = LowerBoundOfLine(file_location_cache, tok)) {
//          line_lbs
//        }
//      }
//    }
//
//    for (unsigned token_index : line.token_index) {
//      Token tok = res.tokens[token_index];
//      VariantId vid = tok.id().Unpack();
//    }
//  }
//}

}  // namespace

void GetIndexedTokenRangeData(
    QPromise<IDatabase::IndexedTokenRangeDataResult> &result_promise,
    const Index &, const FileLocationCache &file_location_cache,
    TokenTree tree, const std::unique_ptr<TokenTreeVisitor> &visitor) {

  IndexedTokenRangeData res;
  std::optional<Fragment> frag = Fragment::containing(tree);
  std::optional<File> file = File::containing(tree);
  if (frag) {
    res.requested_id = frag->id().Pack();

  } else if (file) {
    res.requested_id = file->id().Pack();
  }

  res.tokens = tree.serialize(*visitor);

  res.lines.emplace_back();

  unsigned tok_index = 0u;
  for (Token tok : res.tokens) {
    RenderToken(file_location_cache, res, std::move(tok), tok_index++);
  }

  if (res.lines.back().columns.empty()) {
    res.lines.pop_back();
  }

  result_promise.addResult(std::move(res));
}

}  // namespace mx::gui
