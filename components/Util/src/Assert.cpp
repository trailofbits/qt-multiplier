/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/GUI/Assert.h>

#include <iostream>

namespace mx::gui {

void AssertEx(bool condition, const std::string &file_name,
              std::size_t line_number, const std::string &function_name,
              const std::string &message) {

  if (condition) {
    return;
  }

  std::cerr << file_name << "@" << line_number << "::" << function_name << ": "
            << message << std::endl;

  std::abort();
}

}  // namespace mx::gui
