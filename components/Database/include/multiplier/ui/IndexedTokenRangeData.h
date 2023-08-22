// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/Entities/TokenCategory.h>

#include <multiplier/Types.h>

#include <optional>
#include <vector>

#include <QString>

#include <xxhash.h>

namespace mx::gui {

// TODO(pag): `IndexedTokenRangeData` should split into lines of tokens.
//            Have an algorithm for figuring out line number. The algorithm
//            can start by using lower bound line numbers, and assigning those
//            that are possible, then use `tok.file_token()`, which may point
//            into things like macro arguments to try to get those.

//! The output of a file or fragment database request
class IndexedTokenRangeData final {
 public:
  //! Entity ID associated with the request.
  RawEntityId requested_id{kInvalidEntityId};

  //! Entity ID associated with the response.
  RawEntityId response_id{kInvalidEntityId};

  //! File ID associated with the "base" tokens. This affects line numbering.
  RawEntityId file_id{kInvalidEntityId};

  //! Range of tokens in this data.
  TokenRange tokens;

  struct Column {
    //! Index of this token in `IndexedTokenRangeData::tokens`.
    unsigned token_index{0u};

    //! Did this token start on this line?
    bool starts_on_line{true};

    //! Was this column split across multiple lines?
    bool split_across_lines{false};

    //! Cached copy of the category of this token.
    TokenCategory category{TokenCategory::UNKNOWN};

    //! Data of this token.
    QString data;
  };

  struct Line {
    //! A hash of both the line contents and entity IDs. Used for diffing
    XXH64_hash_t hash{};

    //! Offset of the first QChar of this line.
    unsigned offset{0u};

    //! Optional line number, with `0` signifying absence.
    unsigned number{0u};

    // Each token in this line.
    std::vector<Column> columns;
  };

  //! Lines of tokens.
  std::vector<Line> lines;
};

}  // namespace mx::gui
