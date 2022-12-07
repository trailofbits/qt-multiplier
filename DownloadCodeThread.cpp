// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "CodeView.h"
#include "DownloadCodeThread.h"

#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPlainTextEdit>
#include <QPlainTextDocumentLayout>
#include <QString>
#include <QStringRef>
#include <QTextBlock>
#include <QTextDocument>
#include <QThreadPool>

#include <atomic>
#include <cassert>
#include <cmath>
#include <iostream>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

#include <multiplier/AST.h>
#include <multiplier/Index.h>
#include <multiplier/Util.h>
#include <multiplier/CodeTheme.h>

#include "Code.h"

namespace mx::gui {
namespace {

enum class CodeViewState {
  kInitialized,
  kDownloading,
  kRendering,
  kRendered,
  kFailed
};

}  // namespace

struct DownloadCodeThread::PrivateData {
  const Index index;
  const CodeTheme &theme;
  const FileLocationCache locs;
  const uint64_t counter;

  std::optional<RawEntityId> file_id;
  std::optional<RawEntityId> fragment_id;
  std::optional<std::pair<RawEntityId, RawEntityId>> token_range;

  std::map<RawEntityId, std::vector<TokenList>> fragment_tokens;
  TokenRange file_tokens;

  inline explicit PrivateData(Index index_, const CodeTheme &theme_,
                              const FileLocationCache &locs_,
                              uint64_t counter_)
      : index(std::move(index_)),
        theme(theme_),
        locs(locs_),
        counter(counter_) {}

  bool DownloadFileTokens(void);
  bool DownloadFragmentTokens(void);
  bool DownloadRangeTokens(void);
};

DownloadCodeThread::DownloadCodeThread(PrivateData *d_)
    : d(std::move(d_)) {
  setAutoDelete(true);
}

DownloadCodeThread::~DownloadCodeThread(void) {}

DownloadCodeThread *DownloadCodeThread::CreateFileDownloader(
    const Index &index_, const CodeTheme &theme_,
    const FileLocationCache &locs_, uint64_t counter_,
    RawEntityId file_id_) {
  auto d = new PrivateData(index_, theme_, locs_, counter_);
  d->file_id.emplace(file_id_);
  return new DownloadCodeThread(d);
}

DownloadCodeThread *DownloadCodeThread::CreateFragmentDownloader(
    const Index &index_, const CodeTheme &theme_,
    const FileLocationCache &locs_, uint64_t counter_,
    RawEntityId frag_id_) {
  auto d = new PrivateData(index_, theme_, locs_, counter_);
  d->fragment_id.emplace(frag_id_);
  return new DownloadCodeThread(d);
}

DownloadCodeThread *DownloadCodeThread::CreateTokenRangeDownloader(
    const Index &index_, const CodeTheme &theme_,
    const FileLocationCache &locs_, uint64_t counter_,
    RawEntityId begin_tok_id, RawEntityId end_tok_id) {

  auto d = new PrivateData(index_, theme_, locs_, counter_);
  d->token_range.emplace(begin_tok_id, end_tok_id);
  return new DownloadCodeThread(d);
}

bool DownloadCodeThread::PrivateData::DownloadFileTokens(void) {
  auto file = index.file(file_id.value());
  if (!file) {
    return false;
  }

  file_tokens = file->tokens();

  // Download all of the fragments and build an index of the starting
  // locations of each fragment in this file.
  for (auto fragment : Fragment::in(file.value())) {
    for (Token tok : fragment.file_tokens()) {
      fragment_tokens[tok.id()].emplace_back(fragment.parsed_tokens());
      break;
    }
  }

  return true;
}

bool DownloadCodeThread::PrivateData::DownloadFragmentTokens(void) {
  auto fragment = index.fragment(fragment_id.value());
  if (!fragment) {
    return false;
  }

  file_tokens = fragment->file_tokens();
  for (const Token &tok : file_tokens) {
    fragment_tokens[tok.id()].emplace_back(fragment->parsed_tokens());
    break;
  }

  return true;
}

bool DownloadCodeThread::PrivateData::DownloadRangeTokens(void) {
  VariantId begin_vid = EntityId(token_range->first).Unpack();
  VariantId end_vid = EntityId(token_range->second).Unpack();

  if (begin_vid.index() != end_vid.index()) {
    return false;
  }

  unsigned begin_offset = 0;
  unsigned end_offset = 0;

  // Show a range of file tokens.
  if (std::holds_alternative<FileTokenId>(begin_vid) &&
      std::holds_alternative<FileTokenId>(end_vid)) {

    FileTokenId begin_fid = std::get<FileTokenId>(begin_vid);
    FileTokenId end_fid = std::get<FileTokenId>(end_vid);

    if (begin_fid.file_id != end_fid.file_id ||
        begin_fid.offset > end_fid.offset) {
      return false;
    }

    file_id.emplace(begin_fid.file_id);
    if (!DownloadFileTokens()) {
      return false;
    }

    file_tokens = file_tokens.slice(begin_fid.offset, end_fid.offset + 1u);
    return true;

  // Show a range of fragment tokens.
  } else if (std::holds_alternative<ParsedTokenId>(begin_vid) &&
             std::holds_alternative<ParsedTokenId>(end_vid)) {

    ParsedTokenId begin_fid = std::get<ParsedTokenId>(begin_vid);
    ParsedTokenId end_fid = std::get<ParsedTokenId>(end_vid);

    if (begin_fid.fragment_id != end_fid.fragment_id ||
        begin_fid.offset > end_fid.offset) {
      return false;
    }

    fragment_id.emplace(EntityId(FragmentId(begin_fid.fragment_id)));
    if (!DownloadFragmentTokens()) {
      return false;
    }

    file_tokens = file_tokens.slice(begin_fid.offset, end_fid.offset + 1u);
    return true;

  } else {
    return false;
  }
}

void DownloadCodeThread::run(void) {

  if (d->file_id) {
    if (!d->DownloadFileTokens()) {
      emit DownloadFailed();
      return;
    }
  } else if (d->fragment_id) {
    if (!d->DownloadFragmentTokens()) {
      emit DownloadFailed();
      return;
    }
  } else if (d->token_range) {
    if (!d->DownloadRangeTokens()) {
      emit DownloadFailed();
      return;
    }
  }

  const auto num_file_tokens = d->file_tokens.size();
  if (!num_file_tokens) {
    emit DownloadFailed();
    return;
  }

  auto code = new Code;

  d->theme.BeginTokens();

  code->data.reserve(static_cast<int>(d->file_tokens.data().size()));
  code->bold.reserve(num_file_tokens);
  code->italic.reserve(num_file_tokens);
  code->underline.reserve(num_file_tokens);
  code->foreground.reserve(num_file_tokens);
  code->background.reserve(num_file_tokens);
  code->start_of_token.reserve(num_file_tokens + 1u);
  code->file_token_ids.reserve(num_file_tokens);
  code->tok_decl_ids_begin.reserve(num_file_tokens + 1);

  // Figure out min and max line numbers.
  if (d->file_tokens) {
    if (auto first_loc = d->file_tokens.front().location(d->locs)) {
      code->first_line = first_loc->first;
    }
    if (auto last_loc = d->file_tokens.back().next_location(d->locs)) {
      code->last_line = last_loc->first;
    }
  }

  std::map<RawEntityId, std::vector<Token>> file_to_frag_toks;
  std::vector<Decl> tok_decls;

  RawEntityId last_file_tok_id = kInvalidEntityId;
  for (Token file_tok : d->file_tokens) {
    RawEntityId file_tok_id = file_tok.id();

    // Sortedness needed for `CodeView::ScrollToToken`.
    assert(last_file_tok_id < file_tok_id);
    last_file_tok_id = file_tok_id;

    // This token corresponds to the beginning of a fragment. We might have a
    // one-to-many mapping of file tokens to fragment tokens. So when we come
    // across the first token
    if (auto fragment_tokens_it = d->fragment_tokens.find(file_tok_id);
        fragment_tokens_it != d->fragment_tokens.end()) {
      for (const TokenList &parsed_toks : fragment_tokens_it->second) {
        for (Token parsed_tok : parsed_toks) {
          if (auto file_tok_of_parsed_tok = parsed_tok.file_token()) {
            file_to_frag_toks[file_tok_of_parsed_tok->id()].push_back(parsed_tok);
          }
        }
      }

      d->fragment_tokens.erase(file_tok_id);  // Garbage collect.
    }

    auto tok_start = code->data.size();

    bool is_empty = true;
    bool is_whitespace = true;
    std::string_view utf8_tok = file_tok.data();
    int num_utf8_bytes = static_cast<int>(utf8_tok.size());
    for (QChar ch : QString::fromUtf8(utf8_tok.data(), num_utf8_bytes)) {
      switch (ch.unicode()) {
        case QChar::Tabulation:
          is_empty = false;
          code->data.append(QChar::Tabulation);
          break;
        case QChar::Space:
        case QChar::Nbsp:
          is_empty = false;
          code->data.append(QChar::Space);
          break;
        case QChar::ParagraphSeparator:
        case QChar::LineFeed:
        case QChar::LineSeparator:
          is_empty = false;
          code->data.append(QChar::LineSeparator);
          break;
        case QChar::CarriageReturn:
          continue;
        default:
          code->data.append(ch);
          is_empty = false;
          is_whitespace = false;
          break;
      }
    }

    if (is_empty) {
      continue;
    }

    tok_decls.clear();

    // This is a template of sorts for this location.
    code->file_token_ids.push_back(file_tok_id);
    code->tok_decl_ids_begin.push_back(
        static_cast<unsigned>(code->tok_decl_ids.size()));

    DeclCategory category = DeclCategory::UNKNOWN;
    TokenClass file_tok_class = ClassifyToken(file_tok);

    auto has_added_decl = false;

    // Try to find all declarations associated with this token. There could be
    // multiple if there are multiple fragments overlapping this specific piece
    // of code. However, just because there are multiple fragments, doesn't mean
    // the related declarations are unique.
    if (auto frag_tok_it = file_to_frag_toks.find(file_tok_id);
        frag_tok_it != file_to_frag_toks.end()) {

      for (const Token &frag_tok : frag_tok_it->second) {

        if (auto related_decl = DeclForToken(frag_tok)) {

          // Don't repeat the same declarations.
          //
          // TODO(pag): Investigate this related to the diagnosis in
          //            Issue #118.
          if (has_added_decl &&
              code->tok_decl_ids.back().second == related_decl->id()) {
            continue;
          }

          code->tok_decl_ids.emplace_back(frag_tok.id(), related_decl->id());
          tok_decls.emplace_back(related_decl.value());
          has_added_decl = true;

          // Take the first category we get.
          if (category == DeclCategory::UNKNOWN) {
            category = related_decl->category();
          }

        } else {
          code->tok_decl_ids.emplace_back(frag_tok.id(), kInvalidEntityId);
        }

        // Try to make a better default classification of this token (for syntax
        // coloring in the absence of declaration info).
        if (TokenClass frag_tok_class = ClassifyToken(frag_tok);
            frag_tok_class != file_tok_class &&
            frag_tok_class != TokenClass::kUnknown &&
            frag_tok_class != TokenClass::kIdentifier) {
          file_tok_class = frag_tok_class;
        }
      }

      file_to_frag_toks.erase(file_tok_id);  // Garbage collect.
    }

    TokenCategory kind = CategorizeToken(file_tok, file_tok_class, category);

    code->start_of_token.push_back(tok_start);
    auto [b, i, u] = d->theme.Format(file_tok, tok_decls, kind);
    code->bold.push_back(b);
    code->italic.push_back(i);
    code->underline.push_back(u);
    code->foreground.push_back(&(d->theme.TokenForegroundColor(
        file_tok, tok_decls, kind)));
    code->background.push_back(&(d->theme.TokenBackgroundColor(
        file_tok, tok_decls, kind)));
  }

  code->start_of_token.push_back(code->data.size());
  code->tok_decl_ids_begin.push_back(
      static_cast<unsigned>(code->tok_decl_ids.size()));

  d->theme.EndTokens();

  // We've now rendered the HTML.
  emit RenderCode(std::move(code), d->counter);
}

}  // namespace mx::gui
