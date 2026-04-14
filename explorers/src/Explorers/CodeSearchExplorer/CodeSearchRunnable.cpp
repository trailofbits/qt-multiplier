// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "CodeSearchRunnable.h"
#include "CodeSearchResultsModel.h"

#include <multiplier/Frontend/File.h>
#include <multiplier/Frontend/Query.h>
#include <multiplier/Frontend/TokenTree.h>
#include <multiplier/Fragment.h>
#include <multiplier/GUI/Util.h>

namespace mx::gui {
namespace {

static constexpr size_t kMaxBatchSize = 250u;

// Find a navigable token at `ptr`. Tries parsed tokens first, then
// serialized token tree tokens, then falls back to the file token.
static std::optional<Token> FindParsedTokenAt(
    const char *ptr, const File &file,
    const std::optional<Fragment> &frag) {

  // Find the file token at this pointer.
  std::optional<Token> found_file_tok;
  for (Token file_tok : file.tokens()) {
    std::string_view tok_data = file_tok.data();
    const char *tok_begin = tok_data.data();
    const char *tok_end = tok_begin + tok_data.size();
    if (ptr >= tok_begin && ptr < tok_end) {
      found_file_tok = file_tok;
      break;
    }
  }

  if (!found_file_tok) {
    return std::nullopt;
  }

  if (frag) {
    RawEntityId target_id = found_file_tok->id().Pack();

    // Try parsed tokens first.
    for (Token parsed_tok : frag->parsed_tokens()) {
      Token ft = parsed_tok.file_token();
      if (ft && ft.id().Pack() == target_id) {
        return parsed_tok;
      }
    }

    // Try serialized token tree (pre-expansion tokens).
    TokenTree tt = TokenTree::create(frag.value());
    TokenRange serialized = tt.serialize();
    for (Token tok : serialized) {
      Token ft = tok.file_token();
      if (ft && ft.id().Pack() == target_id) {
        return tok;
      }
    }
  }

  return found_file_tok;
}

// Build a TokenRange covering the text range [data_begin, data_begin + data_len)
// within the file's token data. Tries parsed tokens first, then pre-expansion
// tokens, then falls back to file tokens.
static TokenRange BuildTokenRange(
    const char *data_begin, size_t data_len,
    const File &file, const std::optional<Fragment> &frag) {

  if (!data_len) {
    return TokenRange();
  }

  const char *data_end = data_begin + data_len;

  // Collect file tokens that overlap the match range.
  std::vector<Token> overlapping_file_toks;
  for (Token file_tok : file.tokens()) {
    std::string_view tok_data = file_tok.data();
    const char *tok_begin = tok_data.data();
    const char *tok_end = tok_begin + tok_data.size();

    if (tok_end <= data_begin) continue;
    if (tok_begin >= data_end) break;

    overlapping_file_toks.push_back(file_tok);
  }

  if (overlapping_file_toks.empty()) {
    return TokenRange();
  }

  // Build a set of file token IDs for lookup.
  std::vector<RawEntityId> file_tok_ids;
  file_tok_ids.reserve(overlapping_file_toks.size());
  for (const auto &ft : overlapping_file_toks) {
    file_tok_ids.push_back(ft.id().Pack());
  }

  if (frag) {
    // Try parsed tokens first.
    std::vector<CustomToken> result_toks;
    for (Token parsed_tok : frag->parsed_tokens()) {
      Token ft = parsed_tok.file_token();
      if (ft) {
        RawEntityId ft_id = ft.id().Pack();
        if (std::find(file_tok_ids.begin(), file_tok_ids.end(), ft_id)
            != file_tok_ids.end()) {
          result_toks.emplace_back(std::move(parsed_tok));
        }
      }
    }
    if (!result_toks.empty()) {
      return TokenRange::create(std::move(result_toks));
    }

    // Try serialized token tree (pre-expansion tokens, catches #if 0 etc.).
    TokenTree tt = TokenTree::create(frag.value());
    TokenRange serialized = tt.serialize();
    for (Token tok : serialized) {
      Token ft = tok.file_token();
      if (ft) {
        RawEntityId ft_id = ft.id().Pack();
        if (std::find(file_tok_ids.begin(), file_tok_ids.end(), ft_id)
            != file_tok_ids.end()) {
          result_toks.emplace_back(std::move(tok));
        }
      }
    }
    if (!result_toks.empty()) {
      return TokenRange::create(std::move(result_toks));
    }
  }

  // Fall back to file tokens.
  std::vector<CustomToken> result_toks;
  for (auto &ft : overlapping_file_toks) {
    result_toks.emplace_back(std::move(ft));
  }
  return TokenRange::create(std::move(result_toks));
}

}  // namespace

CodeSearchRunnable::~CodeSearchRunnable(void) {}

CodeSearchRunnable::CodeSearchRunnable(
    RegexQuery query_, Index index_, FileLocationCache cache_,
    AtomicU64Ptr version_)
    : query(std::move(query_)),
      index(std::move(index_)),
      file_location_cache(std::move(cache_)),
      version_number(std::move(version_)),
      captured_version_number(version_number->load()) {
  setAutoDelete(true);
}

void CodeSearchRunnable::run(void) {
  QVector<CodeSearchResultRow> batch;

  for (File file : index.files()) {
    if (version_number->load() != captured_version_number) {
      emit Finished();
      return;
    }

    // Get a display path for the file.
    QString file_path;
    for (auto path : file.paths()) {
      file_path = QString::fromStdString(path.generic_string());
      break;
    }

    for (RegexQueryMatch match : query.match_fragments(file)) {
      if (version_number->load() != captured_version_number) {
        emit Finished();
        return;
      }

      CodeSearchResultRow row;
      row.file = file;
      row.fragment = Fragment::containing(match);

      // Store the matched text data.
      std::string_view match_data = match.data();
      row.match_data = QString::fromUtf8(
          match_data.data(), static_cast<qsizetype>(match_data.size()));

      // Build syntax-highlighted token range for the match.
      row.match_tokens = BuildTokenRange(
          match_data.data(), match_data.size(), file, row.fragment);

      // Find the parsed token at the start of the match for navigation.
      row.location = file_path;
      row.match_token = FindParsedTokenAt(
          match_data.data(), file, row.fragment);
      if (row.match_token) {
        Token loc_tok = row.match_token->file_token();
        if (!loc_tok) {
          loc_tok = row.match_token.value();
        }
        auto loc = LocationOfEntity(file_location_cache,
                                    VariantEntity(loc_tok));
        if (!loc.isEmpty()) {
          row.location = loc;
        }
      }

      // Collect capture groups (skip group 0, which is the full match).
      size_t num_captures = match.num_captures();
      for (size_t i = 1u; i < num_captures; ++i) {
        auto cap_data = match.captured_data(i);
        if (cap_data && !cap_data->empty()) {
          row.capture_data.push_back(QString::fromUtf8(
              cap_data->data(),
              static_cast<qsizetype>(cap_data->size())));
          row.capture_tokens.push_back(
              FindParsedTokenAt(cap_data->data(), file, row.fragment));
          row.capture_token_ranges.push_back(
              BuildTokenRange(cap_data->data(), cap_data->size(),
                              file, row.fragment));
        } else {
          row.capture_data.push_back(QString());
          row.capture_tokens.push_back(std::nullopt);
          row.capture_token_ranges.push_back(TokenRange());
        }
      }

      batch.push_back(std::move(row));

      if (static_cast<size_t>(batch.size()) >= kMaxBatchSize) {
        emit NewResults(captured_version_number, std::move(batch));
        batch.clear();
      }
    }
  }

  if (version_number->load() == captured_version_number && !batch.isEmpty()) {
    emit NewResults(captured_version_number, std::move(batch));
  }

  emit Finished();
}

}  // namespace mx::gui
