// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "MetaTypes.h"
#include "MultiplierStyle.h"

#include <multiplier/Index.h>

#include <QApplication>
#include <QProxyStyle>

#include <phantom/phantomstyle.h>

#include "MainWindow.h"

int main(int argc, char *argv[]) {

  // The PhantomStyle does not really work well on Linux
#ifndef __linux__
  QStyle *phantom_style = new PhantomStyle;
  QStyle *mx_style = new mx::gui::MultiplierStyle(phantom_style);
  QApplication::setStyle(mx_style);
#endif

  QApplication application(argc, argv);
  application.setOrganizationName("Trail of Bits");
  application.setOrganizationDomain("trailofbits.com");
  application.setApplicationName("Multiplier");

  mx::gui::RegisterMetaTypes();

  mx::gui::MainWindow main_window(application);
  main_window.show();

  return application.exec();
}
