// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/Types.h>

#include <QBrush>
#include <QString>
#include <vector>

namespace mx::gui {

class Code {
 public:
  QString data;
  std::vector<int> start_of_token;
  std::vector<RawEntityId> file_token_ids;
  std::vector<std::pair<RawEntityId, RawEntityId>> tok_decl_ids;
  std::vector<unsigned> tok_decl_ids_begin;

  // Line number of the first character of the first token.
  unsigned first_line{0u};

  // Line number of the last character of the last token.
  unsigned last_line{0u};
};

}  // namespace mx
