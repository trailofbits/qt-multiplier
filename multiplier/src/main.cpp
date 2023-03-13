// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "MainWindow.h"
#include "meta_types.h"

#include <multiplier/ui/FontDatabase.h>

#include <QApplication>

#include <phantom/phantomstyle.h>

int main(int argc, char *argv[]) {
  QApplication::setStyle(new PhantomStyle);
  QApplication application(argc, argv);

  mx::gui::RegisterMetaTypes();
  mx::gui::InitializeFontDatabase();

  mx::gui::MainWindow main_window;
  main_window.show();

  return application.exec();
}
