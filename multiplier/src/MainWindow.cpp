// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "MainWindow.h"

#include <QDebug>
#include <QFontDatabase>
#include <QMetaType>
#include <QPixmap>

#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <multiplier/Index.h>
#include <system_error>
#include <tuple>
#include <variant>

namespace mx::gui {

struct MainWindow::PrivateData final {};

MainWindow::MainWindow() : QMainWindow(nullptr), d(new PrivateData) {}

MainWindow::~MainWindow() {}

}  // namespace mx::gui
