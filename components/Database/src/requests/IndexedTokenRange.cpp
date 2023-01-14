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
  output.entity_ids_begin.reserve(num_file_tokens + 1);
  output.token_category_list.reserve(num_file_tokens);

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
  std::vector<RawEntityId> tok_entities;

  RawEntityId opt_last_file_tok_id = kInvalidEntityId;

  for (const Token &file_tok : token_range_data.file_tokens) {
    if (result_promise.isCanceled()) {
      result_promise.addResult(RPCErrorCode::Interrupted);
      return;
    }

    const RawEntityId file_tok_id = file_tok.id().Pack();

    if (file_tok_id >= opt_last_file_tok_id) {
      std::cerr << "Invalid precondition in " __FILE__ << ':' << __LINE__
                << "\nLast file toke id was " << opt_last_file_tok_id
                << " and current is " << file_tok_id << "\n\n";
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
    }

    std::string_view utf8_tok = file_tok.data();
    int num_utf8_bytes = static_cast<int>(utf8_tok.size());
    int tok_start = static_cast<int>(output.data.size());

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

    TokenCategory category = file_tok.category();

    tok_entities.clear();

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

        // Try to make a better default classification of this token (for syntax
        // coloring in the absence of declaration info).
        category = frag_tok.category();

        // If this token has a related entity id, then keep track of it.
        if (auto related_entity_id = frag_tok.related_entity_id()) {
          tok_entities.push_back(related_entity_id.Pack());
        }
      }

      file_to_frag_toks.erase(file_tok_id);  // Garbage collect.
    }

    // This is a template of sorts for this location.
    output.file_token_ids.push_back(file_tok_id);
    output.entity_ids_begin.push_back(
        static_cast<std::uint64_t>(output.entity_ids.size()));

    // Add in the related entity IDs.
    if (!tok_entities.empty()) {
      std::sort(tok_entities.begin(), tok_entities.end());
      auto end = std::unique(tok_entities.begin(), tok_entities.end());
      for (auto it = tok_entities.begin(); it != end; ++it) {
        output.entity_ids.push_back(*it);
      }
    }

    output.start_of_token.push_back(static_cast<int>(tok_start));
    output.token_category_list.push_back(category);
  }

  output.start_of_token.push_back(static_cast<int>(output.data.size()));
  output.entity_ids_begin.push_back(
      static_cast<unsigned>(output.entity_ids.size()));

  result_promise.addResult(std::move(output));
}

}  // namespace mx::gui
