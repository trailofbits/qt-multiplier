/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/ui/RPC.h>

namespace mx::gui {

Result<TokenRangeData, RPCErrorCode>
DownloadEntityTokens(const Index &index, DownloadRequestType request_type,
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

    for (auto fragment : Fragment::in(file.value())) {
      for (const auto &tok : fragment.file_tokens()) {
        output.fragment_tokens[tok.id()].emplace_back(fragment.parsed_tokens());
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

    for (const auto &tok : output.file_tokens) {
      output.fragment_tokens[tok.id()].emplace_back(fragment->parsed_tokens());
      break;
    }

  } else {
    return RPCErrorCode::InvalidDownloadRequestType;
  }

  return output;
}

Result<TokenRangeData, RPCErrorCode>
DownloadTokenRange(const Index &index, RawEntityId start_entity_id,
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

    auto output_res = DownloadEntityTokens(
        index, DownloadRequestType::FileTokens, begin_fid.file_id);

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

    RawEntityId entity_id{EntityId(FragmentId(begin_fid.fragment_id))};
    auto output_res = DownloadEntityTokens(
        index, DownloadRequestType::FragmentTokens, entity_id);

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

}  // namespace mx::gui
