// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "MainWindow.h"
#include "meta_types.h"
#include "Style.h"
#include "Theme.h"

#ifdef __APPLE__
#  include "macos_utils.h"
#endif

#include <multiplier/ui/FontDatabase.h>
#include <multiplier/ui/CodeViewTheme.h>

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QProxyStyle>
#include <QTabBar>

#include <phantom/phantomstyle.h>

int main(int argc, char *argv[]) {
  QStyle *phantom_style = new PhantomStyle;
  QStyle *mx_style = new mx::gui::MultiplierStyle(phantom_style);
  QApplication::setStyle(mx_style);
  QApplication application(argc, argv);
  application.setApplicationName("Multiplier");

  auto do_dark_theme = [&] (void) {
    mx::gui::gUseDarkTheme = true;
#ifdef __APPLE__
    mx::gui::SetNSAppTheme(mx::gui::NSAppTheme::Dark);
#else
    application.setPalette(mx::gui::GetDarkPalette());
#endif
  };

  auto do_light_theme = [&] (void) {
    mx::gui::gUseDarkTheme = false;
#ifdef __APPLE__
    mx::gui::SetNSAppTheme(mx::gui::NSAppTheme::Light);
#else
    application.setPalette(mx::gui::GetLightPalette());
#endif
  };

  QCommandLineParser parser;
  QCommandLineOption theme_option("theme");
  theme_option.setValueName("theme");
  parser.addOption(theme_option);
  parser.process(application);

  auto set_theme = false;
  if (parser.isSet(theme_option)) {
    QString theme_name = parser.value(theme_option);
    if (theme_name.toLower() == "dark") {
      do_dark_theme();
      set_theme = true;
    } else if (theme_name.toLower() == "light") {
      do_light_theme();
      set_theme = true;
    }
  }

  if (!set_theme) {
    do_dark_theme();
  }

  mx::gui::RegisterMetaTypes();
  mx::gui::InitializeFontDatabase();

  mx::gui::MainWindow main_window;
  main_window.show();

  return application.exec();
}
