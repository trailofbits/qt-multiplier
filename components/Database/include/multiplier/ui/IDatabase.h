// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/Types.h>
#include <multiplier/Index.h>
#include <multiplier/Util.h>

#include <QFuture>
#include <QFutureWatcher>
#include <QString>

#include <variant>
#include <vector>

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

struct LineNumberInfo final {
  unsigned first_line{};
  unsigned last_line{};
};

struct IndexedTokenRangeData final {

  using OptionalLineNumberInfo = std::optional<LineNumberInfo>;

  // The data of all of the tokens.
  QString data;

  // The starting index in `data` of the Nth token. There is one extra entry in
  // here to enable quickselect-based searches to map byte offset to token.
  std::vector<unsigned> start_of_token;

  // List of fragment IDs in this token range, and then a compressed mapping
  // of token to fragment ID.
  std::vector<RawEntityId> fragment_ids;
  std::vector<unsigned> fragment_id_index;

  // The line number of a the Nth token.
  std::vector<unsigned> line_number;

  // Token IDs for each of the tokens in `data`. Token IDs might repeat if a
  // given token has more than one line in it. If a token has data a line break
  // in it, then it is split at the line break. E.g. `foo\nbar\nbaz` results in
  // three token chunks: `foo\n`, `bar\n`, and `baz`.
  std::vector<RawEntityId> token_ids;

  // The related entity ID for the Nth token. This is often declaration ID or a
  // macro ID.
  std::vector<RawEntityId> related_entity_ids;

  // The category of the Nth token. The category informs things like syntax
  // coloring.
  std::vector<TokenCategory> token_categories;
};

class IDatabase {
 public:
  using Ptr = std::unique_ptr<IDatabase>;
  using Result = std::variant<IndexedTokenRangeData, RPCErrorCode>;
  using FutureResult = QFuture<Result>;

  IDatabase() = default;
  virtual ~IDatabase() = default;

  static Ptr Create(const Index &index,
                    const FileLocationCache &file_location_cache);

  virtual FutureResult DownloadFile(PackedFileId file_id) = 0;
  virtual FutureResult DownloadFragment(PackedFragmentId fragment_id) = 0;

  IDatabase(const IDatabase &) = delete;
  IDatabase &operator=(const IDatabase &) = delete;
};

}  // namespace mx::gui
