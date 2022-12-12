/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <unordered_map>

#include <multiplier/Index.h>
#include <multiplier/File.h>
#include <multiplier/Result.h>

namespace mx::gui {

enum class RPCErrorCode {
  NoDataReceived,
  InvalidEntityID,
  InvalidDownloadRequestType,
  IndexMismatch,
  FragmentMismatch,
  InvalidFragmentOffsetRange,
  InvalidTokenRangeRequest,
  FileMismatch,
  InvalidFileOffsetRange,
};

enum class DownloadRequestType {
  FileTokens,
  FragmentTokens,
};

struct TokenRangeData final {
  TokenRange file_tokens;
  std::unordered_map<RawEntityId, std::vector<TokenList>> fragment_tokens;
};

Result<TokenRangeData, RPCErrorCode>
DownloadEntityTokens(const Index &index, DownloadRequestType request_type,
                     RawEntityId entity_id);

Result<TokenRangeData, RPCErrorCode>
DownloadTokenRange(const Index &index, DownloadRequestType request_type,
                   RawEntityId start_entity_id, RawEntityId end_entity_id);

}  // namespace mx::gui
