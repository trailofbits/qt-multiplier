/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "IndexedTokenRange.h"

#include <QString>

#include <unordered_map>

#include <multiplier/Entities/Stmt.h>
#include <multiplier/Entities/Token.h>
#include <multiplier/ui/Assert.h>

namespace mx::gui {

namespace {

struct TokenRangeData final {
  // Entity ID associated with the request.
  RawEntityId requested_id;

  TokenRange file_tokens;

  // This is a mapping of the file token IDs to fragment tokens. The file token
  // IDs are the "left corners" of the fragments: the first token from the
  // fragment than can be directly tied to a file token. The fragment tokens
  // are tokens from the fragment that have
  std::unordered_multimap<RawEntityId, std::vector<Token>> fragment_tokens;
};

static void
PrefetchMacrosFromMacro(const QPromise<IDatabase::FileResult> &result_promise,
                        std::vector<Token> &output, const Macro &macro,
                        RawEntityId &first_fid);

static void
PrefetchMacrosFromNode(const QPromise<IDatabase::FileResult> &result_promise,
                       std::vector<Token> &output, MacroOrToken macro_or_tok,
                       RawEntityId &first_fid) {

  if (std::holds_alternative<Macro>(macro_or_tok)) {
    Macro macro = std::move(std::get<Macro>(macro_or_tok));
    PrefetchMacrosFromMacro(result_promise, output, macro, first_fid);

  } else if (std::holds_alternative<Token>(macro_or_tok)) {
    Token macro_tok = std::move(std::get<Token>(macro_or_tok));
    Token ftok = macro_tok.file_token();
    Assert(ftok,
           "Parsed tokens in the usage of a macro should "
           "have associated file tokens");

    // We've found the "left corner" of the macro expansion. This is the first
    // token of the top-level macro usage.
    if (first_fid == kInvalidEntityId) {
      first_fid = ftok.id().Pack();
    }

    if (Token parsed_tok = macro_tok.parsed_token()) {
      output.emplace_back(std::move(parsed_tok));
    } else {
      output.emplace_back(std::move(macro_tok));
    }
  }
}

void PrefetchMacrosFromMacro(
    const QPromise<IDatabase::FileResult> &result_promise,
    std::vector<Token> &output, const Macro &macro, RawEntityId &first_fid) {
  for (MacroOrToken macro_or_tok : macro.children()) {
    if (result_promise.isCanceled()) {
      return;
    }
    PrefetchMacrosFromNode(result_promise, output, std::move(macro_or_tok),
                           first_fid);
  }
}

// Go fetch all of the macros. We don't actually read these, but we want to
// fetch all the macros here where we can check the status of the promise.
static void PrefetchMacrosFromFragment(
    const QPromise<IDatabase::FileResult> &result_promise,
    std::vector<Token> &output, const Fragment &frag, RawEntityId &first_fid) {
  for (MacroOrToken macro_or_tok : frag.preprocessed_code()) {
    if (result_promise.isCanceled()) {
      return;
    }
    PrefetchMacrosFromNode(result_promise, output, std::move(macro_or_tok),
                           first_fid);
  }
}

using TokenRangeDataOrError = std::variant<TokenRangeData, RPCErrorCode>;

static TokenRangeDataOrError
DownloadEntityTokens(const QPromise<IDatabase::FileResult> &result_promise,
                     const Index &index, DownloadRequestType request_type,
                     RawEntityId entity_id) {

  TokenRangeData output;
  output.requested_id = entity_id;

  std::vector<Token> frag_tokens;

  // Download all tokens from a file. This pulls in all of the file's fragments.
  if (request_type == DownloadRequestType::FileTokens) {
    auto file = index.file(entity_id);
    if (!file) {
      return RPCErrorCode::InvalidEntityID;
    }

    output.file_tokens = file->tokens();

    for (Fragment fragment : file->fragments()) {
      RawEntityId first_fid = kInvalidEntityId;

      PrefetchMacrosFromFragment(result_promise, frag_tokens, fragment,
                                 first_fid);
      if (result_promise.isCanceled()) {
        return RPCErrorCode::Interrupted;
      }

      if (first_fid == kInvalidEntityId || frag_tokens.empty()) {
        return RPCErrorCode::NoDataReceived;
      }

      output.fragment_tokens.emplace(first_fid, std::move(frag_tokens));
    }

    // Download all tokens from a fragment.
  } else if (request_type == DownloadRequestType::FragmentTokens) {
    std::optional<Fragment> fragment = index.fragment(entity_id);
    if (!fragment) {
      return RPCErrorCode::InvalidEntityID;
    }

    output.file_tokens = fragment->file_tokens();

    RawEntityId first_fid = kInvalidEntityId;
    PrefetchMacrosFromFragment(result_promise, frag_tokens, *fragment,
                               first_fid);
    if (result_promise.isCanceled()) {
      return RPCErrorCode::Interrupted;
    }

    if (first_fid == kInvalidEntityId || frag_tokens.empty()) {
      return RPCErrorCode::NoDataReceived;
    }

    output.fragment_tokens.emplace(first_fid, std::move(frag_tokens));

  } else {
    return RPCErrorCode::InvalidDownloadRequestType;
  }

  if (output.file_tokens.empty()) {
    return RPCErrorCode::NoDataReceived;
  }

  return output;
}

using IndexedTokenRangeDataOrError =
    std::variant<IndexedTokenRangeData, RPCErrorCode>;

static void RenderToken(std::string_view utf8_tok, TokenCategory category,
                        RawEntityId fid, RawEntityId related_entity_id,
                        RawEntityId statement_containing_token,
                        unsigned &line_number, unsigned fragment_index,
                        IndexedTokenRangeData &output) {

  unsigned num_utf8_bytes = static_cast<unsigned>(utf8_tok.size());
  unsigned tok_start = static_cast<unsigned>(output.data.size());

  bool is_empty = true;
  for (const QChar ch : QString::fromUtf8(utf8_tok.data(), num_utf8_bytes)) {
    switch (ch.unicode()) {
      case QChar::Tabulation:
        is_empty = false;
        output.data.append(QChar::Tabulation);
        break;

      case QChar::Space:
      case QChar::Nbsp:
        is_empty = false;
        output.data.append(QChar::Space);
        break;

      case QChar::ParagraphSeparator:
      case QChar::LineFeed:
      case QChar::LineSeparator:
        output.data.append(QChar::LineSeparator);
        output.line_number.push_back(line_number++);
        output.related_entity_ids.push_back(related_entity_id);
        output.statement_containing_token.push_back(statement_containing_token);
        output.token_ids.push_back(fid);
        output.start_of_token.push_back(static_cast<unsigned>(tok_start));
        output.token_categories.push_back(category);
        output.fragment_id_index.push_back(fragment_index);

        tok_start = static_cast<unsigned>(output.data.size());
        is_empty = true;
        break;

      case QChar::CarriageReturn: continue;

      // TODO(pag): Consult with QFontMetrics or something else to determine
      //            if this character is visible?
      default:
        output.data.append(ch);
        is_empty = false;
        break;
    }
  }

  if (is_empty) {
    return;
  }

  output.line_number.push_back(line_number);
  output.related_entity_ids.push_back(related_entity_id);
  output.statement_containing_token.push_back(statement_containing_token);
  output.token_ids.push_back(fid);
  output.start_of_token.push_back(static_cast<unsigned>(tok_start));
  output.token_categories.push_back(category);
  output.fragment_id_index.push_back(fragment_index);
}


// Render a file token and update `loc` with an approximation of the effect. If
// the token spans more than one line then we split it into multiple tokens, and
// end the internal tokens with newline characters.
static void RenderFileToken(const Token &tok, unsigned &line_number,
                            unsigned fragment_index,
                            IndexedTokenRangeData &output) {

  RenderToken(tok.data(), tok.category(), tok.id().Pack(),
              tok.related_entity_id().Pack(), kInvalidEntityId, line_number,
              fragment_index, output);
}

static bool
RenderFragmentToken(const TokenRange &input_toks, unsigned &file_tok_index,
                    std::vector<Token> &frag_toks, unsigned &line_number,
                    unsigned fragment_index, IndexedTokenRangeData &output) {
  if (frag_toks.empty() || file_tok_index >= input_toks.size()) {
    return false;  // Done with the fragment.
  }

  Token file_tok = input_toks[file_tok_index];
  if (!file_tok) {
    return false;
  }

  const Token &frag_tok = frag_toks.back();
  Token frag_file_tok = frag_tok.file_token();
  RawEntityId frag_file_tok_id = frag_file_tok.id().Pack();
  RawEntityId file_tok_id = file_tok.id().Pack();

  ++file_tok_index;

  // There isn't a corresponding fragment token, so render the file token,
  // leaving the fragment token in place.
  if (frag_file_tok_id == kInvalidEntityId || frag_file_tok_id > file_tok_id) {
    RenderFileToken(file_tok, line_number, fragment_index, output);
    return true;
  }

  // If the fragment token's file token ID is less than the current file token
  // ID, then somehow we've gone too far or gone out-of-sync.
  Assert(frag_file_tok_id == file_tok_id,
         "File and fragment token ids are out-of-sync.");

  // Go find the first statement that encloses this token.
  RawEntityId statement_id = kInvalidEntityId;
  for (Stmt stmt : Stmt::containing(frag_tok)) {
    statement_id = stmt.id().Pack();
    break;
  }

  // Render out the file token data, annotated with fragment token info.
  RenderToken(file_tok.data(), frag_tok.category(), frag_tok.id().Pack(),
              frag_tok.related_entity_id().Pack(), statement_id, line_number,
              fragment_index, output);

  frag_toks.pop_back();
  return true;
}

// Get the fragment ID from the tokens.
static RawEntityId FragmentIdFromTokens(const std::vector<Token> &frag_toks) {
  RawEntityId fragment_id = kInvalidEntityId;
  for (const Token &ftok : frag_toks) {
    VariantId vid = ftok.id().Unpack();
    if (std::holds_alternative<ParsedTokenId>(vid)) {
      return EntityId(FragmentId(std::get<ParsedTokenId>(vid).fragment_id))
          .Pack();
    } else if (std::holds_alternative<MacroTokenId>(vid)) {
      return EntityId(FragmentId(std::get<MacroTokenId>(vid).fragment_id))
          .Pack();

    } else {
      Assert(false, "Unexpected token in fragmnet token list");
    }
  }
  Assert(fragment_id != kInvalidEntityId, "Could not find fragment id");
  return kInvalidEntityId;
}

// Create an indexed version of some token range data. This means going and
// finding the
static IndexedTokenRangeDataOrError
IndexTokenRange(const QPromise<IDatabase::FileResult> &result_promise,
                const FileLocationCache &file_location_cache,
                TokenRangeData input) {

  IndexedTokenRangeData output;
  output.requested_id = input.requested_id;
  output.fragment_ids.push_back(kInvalidEntityId);

  unsigned line_number = 0u;

  size_t num_file_tokens = input.file_tokens.size();
  for (unsigned tok_index = 0u; tok_index < num_file_tokens;) {
    Token file_tok = input.file_tokens[tok_index];

    if (result_promise.isCanceled()) {
      return RPCErrorCode::Interrupted;
    }

    const RawEntityId file_tok_id = file_tok.id().Pack();

    if (!line_number) {
      auto line_col = file_tok.location(file_location_cache);
      if (line_col) {
        line_number = line_col->first;
      }
    }

    // Find the set of fragment tokens associated with this file token id.
    auto [fragment_tokens_it, fragment_tokens_end] =
        input.fragment_tokens.equal_range(file_tok_id);

    // Easy case: no fragment overlaps with this token.
    if (fragment_tokens_it == fragment_tokens_end) {
      RenderFileToken(file_tok, line_number, 0u, output);
      ++tok_index;
      continue;
    }

    // Hard case: we need to render the fragments out, one after another.
    auto old_line_number = line_number;
    auto old_tok_index = tok_index;
    auto last_tok_index = output.line_number.size();
    for (auto repeat = false; fragment_tokens_it != fragment_tokens_end;
         ++fragment_tokens_it) {

      if (repeat) {

        // Force a newline between overlapping fragments.
        if (!output.data.endsWith(QChar::LineSeparator)) {
          output.data.push_back(QChar::LineSeparator);
        }

        // Back-fill the original tokens from the same line.
        auto j = 0u;
        for (auto i = last_tok_index;
             i-- > 0u && output.line_number[i] == old_line_number; ++j) {
        }

        for (auto i = 0u; i < j; ++i) {
          auto k = last_tok_index - i;
          output.fragment_id_index.push_back(output.fragment_id_index[k]);
          output.token_ids.push_back(output.token_ids[k]);
          output.related_entity_ids.push_back(output.related_entity_ids[k]);
          output.statement_containing_token.push_back(
              output.statement_containing_token[k]);
          output.line_number.push_back(output.line_number[k]);
          output.token_categories.push_back(output.token_categories[k]);
          auto start = static_cast<unsigned>(output.data.size());
          output.start_of_token.push_back(start);
          auto len = output.start_of_token[k + 1u] - output.start_of_token[k];
          output.data.append(output.data.mid(start, len));
        }
      }

      std::vector<Token> fragment_tokens =
          std::move(fragment_tokens_it->second);

      auto frag_index = static_cast<unsigned>(output.fragment_ids.size());
      output.fragment_ids.push_back(FragmentIdFromTokens(fragment_tokens));
      line_number = old_line_number;
      tok_index = old_tok_index;

      // Reverse these so that we can pop them off the end to consume them.
      std::reverse(fragment_tokens.begin(), fragment_tokens.end());

      // Output the file and fragment tokens.
      while (RenderFragmentToken(input.file_tokens, tok_index, fragment_tokens,
                                 line_number, frag_index, output)) {
        if (result_promise.isCanceled()) {
          return RPCErrorCode::Interrupted;
        }
      }

      repeat = true;
    }
  }

  Assert(output.related_entity_ids.size() == output.start_of_token.size(), "");

  Assert(
      output.statement_containing_token.size() == output.start_of_token.size(),
      "");

  Assert(output.token_ids.size() == output.start_of_token.size(), "");

  Assert(output.fragment_id_index.size() == output.start_of_token.size(), "");

  Assert(output.line_number.size() == output.start_of_token.size(), "");

  Assert(output.token_categories.size() == output.start_of_token.size(), "");

  output.start_of_token.push_back(static_cast<unsigned>(output.data.size()));

  return output;
}

}  // namespace

void CreateIndexedTokenRangeData(
    QPromise<IDatabase::FileResult> &result_promise, const Index &index,
    const FileLocationCache &file_location_cache,
    const SingleEntityRequest &request) {

  auto token_range_data_res = DownloadEntityTokens(
      result_promise, index, request.download_request_type, request.entity_id);


  if (std::holds_alternative<RPCErrorCode>(token_range_data_res)) {
    result_promise.addResult(std::get<RPCErrorCode>(token_range_data_res));
    return;
  }

  TokenRangeData token_range_data =
      std::move(std::get<TokenRangeData>(token_range_data_res));

  IndexedTokenRangeDataOrError indexed_tokens_res = IndexTokenRange(
      result_promise, file_location_cache, std::move(token_range_data));

  if (std::holds_alternative<RPCErrorCode>(indexed_tokens_res)) {
    result_promise.addResult(std::get<RPCErrorCode>(token_range_data_res));
    return;
  }

  result_promise.addResult(
      std::move(std::get<IndexedTokenRangeData>(indexed_tokens_res)));
}

}  // namespace mx::gui
