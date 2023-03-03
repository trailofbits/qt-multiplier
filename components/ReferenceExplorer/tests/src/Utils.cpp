/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "Utils.h"

namespace mx::gui {

mx::Index OpenTestDatabase(const std::string &database_name) {

  auto database_path =
      std::filesystem::path(MXQT_CI_DATA_PATH) / database_name / "database.mx";

  return mx::EntityProvider::in_memory_cache(
      mx::EntityProvider::from_database(database_path.string()));
}

}  // namespace mx::gui
