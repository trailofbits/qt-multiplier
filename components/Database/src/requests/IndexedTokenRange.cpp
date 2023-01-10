/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "IndexedTokenRange.h"

#include <iostream>

namespace mx::gui {

namespace {

struct TokenRangeData final {
  TokenRange file_tokens;
  std::unordered_map<RawEntityId, std::vector<TokenRange>> fragment_tokens;
};

Result<TokenRangeData, RPCErrorCode>
DownloadEntityTokens(const IndexedTokenRangeDataResultPromise &result_promise,
                     const Index &index, DownloadRequestType request_type,
                     RawEntityId entity_id) {

  TokenRangeData output;

  if (request_type == DownloadRequestType::FileTokens) {
    auto file = index.file(entity_id);
    if (!file) {
      return RPCErrorCode::InvalidEntityID;
    }

    output.file_tokens = file->tokens();
    if (output.file_tokens.size() == 0) {
      return RPCErrorCode::NoDataReceived;
    }

    for (Fragment fragment : file->fragments()) {
      for (const Token &tok : fragment.file_tokens()) {
        if (result_promise.isCanceled()) {
          return RPCErrorCode::Interrupted;
        }

        RawEntityId raw_tok_id = tok.id().Pack();
        output.fragment_tokens[raw_tok_id].emplace_back(
            fragment.parsed_tokens());
        break;
      }
    }

  } else if (request_type == DownloadRequestType::FragmentTokens) {
    auto fragment = index.fragment(entity_id);
    if (!fragment) {
      return RPCErrorCode::InvalidEntityID;
    }

    output.file_tokens = fragment->file_tokens();
    if (output.file_tokens.size() == 0) {
      return RPCErrorCode::NoDataReceived;
    }

    for (const Token &tok : output.file_tokens) {
      if (result_promise.isCanceled()) {
        return RPCErrorCode::Interrupted;
      }

      RawEntityId raw_tok_id = tok.id().Pack();
      output.fragment_tokens[raw_tok_id].emplace_back(
          fragment->parsed_tokens());
      break;
    }

  } else {
    return RPCErrorCode::InvalidDownloadRequestType;
  }

  return output;
}

Result<TokenRangeData, RPCErrorCode>
DownloadTokenRange(const IndexedTokenRangeDataResultPromise &result_promise,
                   const Index &index, RawEntityId start_entity_id,
                   RawEntityId end_entity_id) {

  VariantId begin_vid = EntityId(start_entity_id).Unpack();
  VariantId end_vid = EntityId(end_entity_id).Unpack();

  if (begin_vid.index() != end_vid.index()) {
    return RPCErrorCode::IndexMismatch;
  }

  // Show a range of file tokens.
  if (std::holds_alternative<FileTokenId>(begin_vid) &&
      std::holds_alternative<FileTokenId>(end_vid)) {

    const auto &begin_fid = std::get<FileTokenId>(begin_vid);
    const auto &end_fid = std::get<FileTokenId>(end_vid);

    if (begin_fid.file_id != end_fid.file_id) {
      return RPCErrorCode::FileMismatch;
    }

    if (begin_fid.offset > end_fid.offset) {
      return RPCErrorCode::InvalidFileOffsetRange;
    }

    RawEntityId entity_id{EntityId(FragmentId(begin_fid.file_id)).Pack()};
    auto output_res = DownloadEntityTokens(
        result_promise, index, DownloadRequestType::FileTokens, entity_id);

    if (!output_res.Succeeded()) {
      return output_res.TakeError();
    }

    auto output = output_res.TakeValue();
    output.file_tokens =
        output.file_tokens.slice(begin_fid.offset, end_fid.offset + 1u);

    return output;

    // Show a range of fragment tokens.
  } else if (std::holds_alternative<ParsedTokenId>(begin_vid) &&
             std::holds_alternative<ParsedTokenId>(end_vid)) {

    ParsedTokenId begin_fid = std::get<ParsedTokenId>(begin_vid);
    ParsedTokenId end_fid = std::get<ParsedTokenId>(end_vid);

    if (begin_fid.fragment_id != end_fid.fragment_id) {
      return RPCErrorCode::FragmentMismatch;
    }

    if (begin_fid.offset > end_fid.offset) {
      return RPCErrorCode::InvalidFragmentOffsetRange;
    }

    RawEntityId entity_id{EntityId(FragmentId(begin_fid.fragment_id)).Pack()};
    auto output_res = DownloadEntityTokens(
        result_promise, index, DownloadRequestType::FragmentTokens, entity_id);

    if (!output_res.Succeeded()) {
      return output_res.TakeError();
    }

    auto output = output_res.TakeValue();
    output.file_tokens =
        output.file_tokens.slice(begin_fid.offset, end_fid.offset + 1u);

    return output;

  } else {
    return RPCErrorCode::InvalidTokenRangeRequest;
  }
}

}  // namespace

void CreateIndexedTokenRangeData(
    IndexedTokenRangeDataResultPromise &result_promise, const Index &index,
    const FileLocationCache &file_location_cache, const Request &request) {

  Result<TokenRangeData, RPCErrorCode> token_range_data_res;
  if (std::holds_alternative<SingleEntityRequest>(request)) {
    const auto &entity_request = std::get<SingleEntityRequest>(request);
    token_range_data_res = DownloadEntityTokens(
        result_promise, index, entity_request.download_request_type,
        entity_request.entity_id);

  } else {
    const auto &entity_range_request = std::get<EntityRangeRequest>(request);
    token_range_data_res = DownloadTokenRange(
        result_promise, index, entity_range_request.start_entity_id,
        entity_range_request.end_entity_id);
  }

  if (!token_range_data_res.Succeeded()) {
    result_promise.addResult(token_range_data_res.TakeError());
    return;
  }

  auto token_range_data = token_range_data_res.TakeValue();

  IndexedTokenRangeData output;
  auto token_data_size =
      static_cast<int>(token_range_data.file_tokens.data().size());

  output.data.reserve(token_data_size);

  auto num_file_tokens = token_range_data.file_tokens.size();
  output.start_of_token.reserve(num_file_tokens + 1);
  output.file_token_ids.reserve(num_file_tokens);
  output.tok_decl_ids.reserve(num_file_tokens);
  output.tok_decl_ids_begin.reserve(num_file_tokens + 1);
  output.token_category_list.reserve(num_file_tokens);
  output.token_decl_list.reserve(num_file_tokens);
  output.token_list.reserve(num_file_tokens);
  output.token_class_list.reserve(num_file_tokens);

  if (token_range_data.file_tokens) {
    const auto &first_loc =
        token_range_data.file_tokens.front().location(file_location_cache);
    const auto &last_loc =
        token_range_data.file_tokens.back().next_location(file_location_cache);

    if (first_loc && last_loc) {
      output.opt_line_number_info = IndexedTokenRangeData::LineNumberInfo{
          first_loc->first, last_loc->first};
    }
  }

  std::unordered_map<RawEntityId, std::vector<Token>> file_to_frag_toks;
  std::vector<Decl> tok_decls;

  bool sortedness_precondition_error{false};

  std::optional<RawEntityId> opt_last_file_tok_id;
  for (const auto &file_tok : token_range_data.file_tokens) {
    if (result_promise.isCanceled()) {
      result_promise.addResult(RPCErrorCode::Interrupted);
      return;
    }

    const RawEntityId file_tok_id = file_tok.id().Pack();

    // Sortedness needed for `CodeView::ScrollToToken`.
    if (opt_last_file_tok_id.has_value() &&
        opt_last_file_tok_id.value() < file_tok_id &&
        !sortedness_precondition_error) {
      // TODO(alessandro): This was originally an assert. It seems like
      // this condition is not met, so it's commented out for now.
      // result_promise.addResult(RPCErrorCode::InvalidFileTokenSorting);
      // return;

      std::cerr << "Invalid precondition in " __FILE__ << "@" << __LINE__
                << "\n";

      sortedness_precondition_error = true;
    }

    opt_last_file_tok_id = file_tok_id;

    // This token corresponds to the beginning of a fragment. We might have a
    // one-to-many mapping of file tokens to fragment tokens. So when we come
    // across the first token
    auto fragment_tokens_it =
        token_range_data.fragment_tokens.find(file_tok_id);
    if (fragment_tokens_it != token_range_data.fragment_tokens.end()) {
      const auto &parsed_token_list = fragment_tokens_it->second;

      for (const TokenRange &parsed_toks : parsed_token_list) {
        for (const Token &parsed_tok : parsed_toks) {
          if (result_promise.isCanceled()) {
            result_promise.addResult(RPCErrorCode::Interrupted);
            return;
          }

          const auto &file_tok_of_parsed_tok = parsed_tok.file_token();

          if (file_tok_of_parsed_tok) {
            const RawEntityId id = file_tok_of_parsed_tok->id().Pack();
            file_to_frag_toks[id].push_back(parsed_tok);
          }
        }
      }

      // Garbage collect.
      //token_range_data.fragment_tokens.erase(file_tok_id);
    }

    auto tok_start = output.data.size();

    const auto &utf8_tok = file_tok.data();
    int num_utf8_bytes = static_cast<int>(utf8_tok.size());

    bool is_empty = true;
    for (const auto &ch : QString::fromUtf8(utf8_tok.data(), num_utf8_bytes)) {
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
          is_empty = false;
          output.data.append(QChar::LineSeparator);
          break;

        case QChar::CarriageReturn: continue;
        default:
          output.data.append(ch);
          is_empty = false;
          break;
      }
    }

    if (is_empty) {
      continue;
    }

    // This is a template of sorts for this location.
    output.file_token_ids.push_back(file_tok_id);
    output.tok_decl_ids_begin.push_back(
        static_cast<std::uint64_t>(output.tok_decl_ids.size()));

    DeclCategory category = DeclCategory::UNKNOWN;
    TokenClass file_tok_class = ClassifyToken(file_tok);

    auto has_added_decl = false;
    tok_decls.clear();

    // Try to find all declarations associated with this token. There could be
    // multiple if there are multiple fragments overlapping this specific piece
    // of code. However, just because there are multiple fragments, doesn't mean
    // the related declarations are unique.
    if (auto frag_tok_it = file_to_frag_toks.find(file_tok_id);
        frag_tok_it != file_to_frag_toks.end()) {

      for (const Token &frag_tok : frag_tok_it->second) {
        if (result_promise.isCanceled()) {
          result_promise.addResult(RPCErrorCode::Interrupted);
          return;
        }

        if (auto related_decl = DeclForToken(frag_tok)) {

          // Don't repeat the same declarations.
          //
          // TODO(pag): Investigate this related to the diagnosis in
          //            Issue #118.
          if (has_added_decl &&
              output.tok_decl_ids.back().second == related_decl->id()) {
            continue;
          }

          output.tok_decl_ids.emplace_back(frag_tok.id().Pack(),
                                           related_decl->id().Pack());
          tok_decls.emplace_back(related_decl.value());
          has_added_decl = true;

          // Take the first category we get.
          if (category == DeclCategory::UNKNOWN) {
            category = related_decl->category();
          }

        } else {
          output.tok_decl_ids.emplace_back(frag_tok.id().Pack(),
                                           kInvalidEntityId);
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

    output.start_of_token.push_back(static_cast<int>(tok_start));
    output.token_category_list.push_back(kind);
    output.token_decl_list.push_back(std::move(tok_decls));
    output.token_list.push_back(file_tok);
    output.token_class_list.push_back(file_tok_class);
  }

  output.start_of_token.push_back(static_cast<int>(output.data.size()));
  output.tok_decl_ids_begin.push_back(
      static_cast<unsigned>(output.tok_decl_ids.size()));

  result_promise.addResult(std::move(output));
}

}  // namespace mx::gui
