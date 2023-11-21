/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <string>

namespace mx::gui {

#define Assert(condition, message) \
  do { \
    AssertEx(condition, __FILE__, __LINE__, __func__, message); \
  } while (false)

void AssertEx(bool condition, const std::string &file_name,
              std::size_t line_number, const std::string &function_name,
              const std::string &message);

}  // namespace mx::gui
