// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/Types.h>
#include <multiplier/Index.h>

#include <QString>

#include <vector>
#include <optional>

namespace mx::gui {

//! The output of a file or fragment database request
struct IndexedTokenRangeData final {
  //! Line number information
  struct LineNumberInfo final {
    unsigned first_line{};
    unsigned last_line{};
  };

  //! An optional line number information object
  using OptionalLineNumberInfo = std::optional<LineNumberInfo>;

  //! Entity ID associated with the request.
  RawEntityId requested_id;

  //! The data of all of the tokens.
  QString data;

  //! \brief The starting index in `data` of the Nth token
  //! There is one extra entry in here to enable quickselect-based searches to map
  //! byte offset to token.
  std::vector<unsigned> start_of_token;

  //! List of fragment IDs in this token range
  std::vector<RawEntityId> fragment_ids;

  //! A compressed mapping of token to fragment ID.
  std::vector<unsigned> fragment_id_index;

  //! The line number of a the Nth token.
  std::vector<unsigned> line_number;

  //! \brief Token IDs for each of the tokens in `data`
  //! Token IDs might repeat if a
  //! given token has more than one line in it. If a token has data a line break
  //! in it, then it is split at the line break. E.g. `foo\nbar\nbaz` results in
  //! three token chunks: `foo\n`, `bar\n`, and `baz`.
  std::vector<RawEntityId> token_ids;

  //! \brief The related entity ID for the Nth token
  //! This is often declaration ID or a macro ID.
  std::vector<RawEntityId> related_entity_ids;

  //! The entity ID of the statement containing the Nth token.
  std::vector<RawEntityId> statement_containing_token;

  //! \brief The category of the Nth token
  //! The category informs things like syntax coloring.
  std::vector<TokenCategory> token_categories;
};

}  // namespace mx::gui
