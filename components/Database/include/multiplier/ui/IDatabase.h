// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/Types.h>
#include <multiplier/Index.h>
#include <multiplier/Util.h>
#include <multiplier/Result.h>

#include <QFuture>
#include <QFutureWatcher>
#include <QString>

namespace mx::gui {

enum class RPCErrorCode {
  Interrupted,
  NoDataReceived,
  InvalidEntityID,
  InvalidDownloadRequestType,
  IndexMismatch,
  FragmentMismatch,
  InvalidFragmentOffsetRange,
  InvalidTokenRangeRequest,
  FileMismatch,
  InvalidFileOffsetRange,
  InvalidFileTokenSorting,
};

struct IndexedTokenRangeData final {
  struct LineNumberInfo final {
    std::uint64_t first_line{};
    std::uint64_t last_line{};
  };

  using OptionalLineNumberInfo = std::optional<LineNumberInfo>;

  QString data;
  std::vector<int> start_of_token;
  std::vector<RawEntityId> file_token_ids;
  std::vector<std::pair<RawEntityId, RawEntityId>> tok_decl_ids;
  std::vector<std::uint64_t> tok_decl_ids_begin;
  std::vector<TokenCategory> token_category_list;
  std::vector<std::vector<Decl>> token_decl_list;
  std::vector<Token> token_list;
  std::vector<TokenClass> token_class_list;
  OptionalLineNumberInfo opt_line_number_info;
};

class IDatabase {
 public:
  using Ptr = std::unique_ptr<IDatabase>;
  using Result = Result<IndexedTokenRangeData, RPCErrorCode>;
  using FutureResult = QFuture<Result>;

  IDatabase() = default;
  virtual ~IDatabase() = default;

  static Ptr Create(const Index &index,
                    const FileLocationCache &file_location_cache);

  virtual FutureResult DownloadFile(PackedFileId file_id) = 0;
  virtual FutureResult DownloadFragment(PackedFragmentId fragment_id) = 0;
  virtual FutureResult DownloadTokenRange(const RawEntityId &start_entity_id,
                                          const RawEntityId &end_entity_id) = 0;

  IDatabase(const IDatabase &) = delete;
  IDatabase &operator=(const IDatabase &) = delete;
};

}  // namespace mx::gui
