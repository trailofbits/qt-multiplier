// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "MainWindow.h"
#include "meta_types.h"
#include "themes.h"

#ifdef __APPLE__
#  include "macos_utils.h"
#endif

#include <multiplier/ui/FontDatabase.h>

#include <QApplication>

#include <phantom/phantomstyle.h>

int main(int argc, char *argv[]) {
  QApplication::setStyle(new PhantomStyle());
  QApplication application(argc, argv);

#ifdef __APPLE__
  mx::gui::SetNSAppTheme(mx::gui::NSAppTheme::Dark);
#else
  application.setPalette(mx::gui::GetDarkPalette());
#endif

  mx::gui::RegisterMetaTypes();
  mx::gui::InitializeFontDatabase();

  mx::gui::MainWindow main_window;
  main_window.show();

  return application.exec();
}
