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
  RawEntityId requested_id;

  //! Range of tokens in this data.
  TokenRange tokens;

  //! Total number of `QChar`s needed to represent all lines of code.
  unsigned size{0u};

  struct Column {
    //! Index of this token in `IndexedTokenRangeData::tokens`.
    unsigned index{0u};

    //! Offset of the first QChar of this token within the overall concatenated
    //! data of
    unsigned offset{0u};

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
    //! Number of QChars of data on this line. This includes any trailing
    //! newline.
    unsigned size{0u};

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
