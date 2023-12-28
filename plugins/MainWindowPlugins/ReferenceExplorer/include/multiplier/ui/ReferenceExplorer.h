// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/ui/IMainWindowPlugin.h>

namespace mx::gui {

class Context;

std::unique_ptr<IMainWindowPlugin> CreateReferenceExplorerMainWindowPlugin(
    const Context &context, QMainWindow *parent);

}  // namespace mx::gui