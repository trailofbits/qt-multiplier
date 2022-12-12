// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "DownloadCodeThread.h"

#include <multiplier/AST.h>
#include <multiplier/Code.h>
#include <multiplier/CodeTheme.h>
#include <multiplier/Index.h>
#include <multiplier/Util.h>
#include <multiplier/ui/RPC.h>

#include <QString>
#include <cassert>
#include <cmath>
#include <vector>

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
  PrivateData(const Index &index_, const CodeTheme &theme_,
              const FileLocationCache &locs_, uint64_t counter_,
              Request request_)
      : index(std::move(index_)),
        theme(theme_),
        locs(locs_),
        counter(counter_),
        request(request_) {}

  const Index &index;
  const CodeTheme &theme;
  const FileLocationCache &locs;
  const uint64_t counter;
  Request request;
};

DownloadCodeThread::~DownloadCodeThread(void) {}

DownloadCodeThread *DownloadCodeThread::CreateFileDownloader(
    const Index &index, const CodeTheme &code_theme,
    const FileLocationCache &file_location_cache, const std::uint64_t &counter,
    const RawEntityId &file_id) {

  SingleEntityRequest request{
      DownloadRequestType::FileTokens,
      file_id,
  };

  return new DownloadCodeThread(index, code_theme, file_location_cache, counter,
                                std::move(request));
}

DownloadCodeThread *DownloadCodeThread::CreateFragmentDownloader(
    const Index &index, const CodeTheme &code_theme,
    const FileLocationCache &file_location_cache, const std::uint64_t &counter,
    const RawEntityId &fragment_id) {

  SingleEntityRequest request{
      DownloadRequestType::FragmentTokens,
      fragment_id,
  };

  return new DownloadCodeThread(index, code_theme, file_location_cache, counter,
                                std::move(request));
}

DownloadCodeThread *DownloadCodeThread::CreateTokenRangeDownloader(
    const Index &index, const CodeTheme &code_theme,
    const FileLocationCache &file_location_cache, const std::uint64_t &counter,
    const RawEntityId &start_entity_id, const RawEntityId &end_entity_id) {

  EntityRangeRequest request{
      start_entity_id,
      end_entity_id,
  };

  return new DownloadCodeThread(index, code_theme, file_location_cache, counter,
                                std::move(request));
}

DownloadCodeThread::DownloadCodeThread(
    const Index &index, const CodeTheme &code_theme,
    const FileLocationCache &file_location_cache, uint64_t counter,
    const Request &request)
    : d(new PrivateData(index, code_theme, file_location_cache, counter,
                        request)) {
  setAutoDelete(true);
}

void DownloadCodeThread::run(void) {
  Result<TokenRangeData, RPCErrorCode> token_range_data_res;
  if (std::holds_alternative<SingleEntityRequest>(d->request)) {
    const auto &request = std::get<SingleEntityRequest>(d->request);
    token_range_data_res = DownloadEntityTokens(
        d->index, request.download_request_type, request.entity_id);

  } else {
    const auto &request = std::get<EntityRangeRequest>(d->request);
    token_range_data_res = DownloadTokenRange(d->index, request.start_entity_id,
                                              request.end_entity_id);
  }

  if (!token_range_data_res.Succeeded()) {
    emit DownloadFailed();
  }

  auto token_range_data = token_range_data_res.TakeValue();
  auto num_file_tokens = token_range_data.file_tokens.size();

  auto code = new Code;

  d->theme.BeginTokens();

  code->data.reserve(
      static_cast<int>(token_range_data.file_tokens.data().size()));
  code->bold.reserve(num_file_tokens);
  code->italic.reserve(num_file_tokens);
  code->underline.reserve(num_file_tokens);
  code->foreground.reserve(num_file_tokens);
  code->background.reserve(num_file_tokens);
  code->start_of_token.reserve(num_file_tokens + 1u);
  code->file_token_ids.reserve(num_file_tokens);
  code->tok_decl_ids_begin.reserve(num_file_tokens + 1);
  code->token_category_list.reserve(num_file_tokens);
  code->token_decl_list.reserve(num_file_tokens);
  code->token_list.reserve(num_file_tokens);
  code->token_class_list.reserve(num_file_tokens);

  // Figure out min and max line numbers.
  if (token_range_data.file_tokens) {
    if (auto first_loc =
            token_range_data.file_tokens.front().location(d->locs)) {
      code->first_line = first_loc->first;
    }
    if (auto last_loc =
            token_range_data.file_tokens.back().next_location(d->locs)) {
      code->last_line = last_loc->first;
    }
  }

  std::unordered_map<RawEntityId, std::vector<Token>> file_to_frag_toks;
  std::vector<Decl> tok_decls;

  RawEntityId last_file_tok_id = kInvalidEntityId;
  for (Token file_tok : token_range_data.file_tokens) {
    RawEntityId file_tok_id = file_tok.id();

    // Sortedness needed for `CodeView::ScrollToToken`.
    assert(last_file_tok_id < file_tok_id);

    // Force as read in case we are not in debug mode and the
    // above assert is not being compiled
    static_cast<void>(last_file_tok_id);

    last_file_tok_id = file_tok_id;

    // This token corresponds to the beginning of a fragment. We might have a
    // one-to-many mapping of file tokens to fragment tokens. So when we come
    // across the first token
    if (auto fragment_tokens_it =
            token_range_data.fragment_tokens.find(file_tok_id);
        fragment_tokens_it != token_range_data.fragment_tokens.end()) {
      for (const TokenList &parsed_toks : fragment_tokens_it->second) {
        for (Token parsed_tok : parsed_toks) {
          if (auto file_tok_of_parsed_tok = parsed_tok.file_token()) {
            file_to_frag_toks[file_tok_of_parsed_tok->id()].push_back(
                parsed_tok);
          }
        }
      }

      token_range_data.fragment_tokens.erase(file_tok_id);  // Garbage collect.
    }

    auto tok_start = code->data.size();

    bool is_empty = true;
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
        case QChar::CarriageReturn: continue;
        default:
          code->data.append(ch);
          is_empty = false;
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
    code->foreground.push_back(
        &(d->theme.TokenForegroundColor(file_tok, tok_decls, kind)));
    code->background.push_back(
        &(d->theme.TokenBackgroundColor(file_tok, tok_decls, kind)));
    code->token_category_list.push_back(kind);
    code->token_decl_list.push_back(std::move(tok_decls));
    code->token_list.push_back(file_tok);
    code->token_class_list.push_back(file_tok_class);
  }

  code->start_of_token.push_back(code->data.size());
  code->tok_decl_ids_begin.push_back(
      static_cast<unsigned>(code->tok_decl_ids.size()));

  d->theme.EndTokens();

  // We've now rendered the HTML.
  emit RenderCode(std::move(code), d->counter);
}

}  // namespace mx::gui
