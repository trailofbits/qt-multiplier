// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "MainWindow.h"
#include "MetaTypes.h"
#include "Style.h"

#include <multiplier/ui/FontDatabase.h>
#include <multiplier/ui/IThemeManager.h>

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QProxyStyle>
#include <QTabBar>
#include <QStringList>

#include <phantom/phantomstyle.h>

namespace {

bool kDefaultToDarkTheme{true};

bool ShouldUseDarkTheme(int argc, char *argv[]) {
  QCommandLineOption theme_option("theme");
  theme_option.setValueName("theme");

  QCommandLineParser parser;
  parser.addOption(theme_option);

  QStringList argument_list;
  for (int i = 0; i < argc; ++i) {
    argument_list.push_back(QString::fromUtf8(argv[i]));
  }

  parser.process(argument_list);

  bool use_dark_theme{kDefaultToDarkTheme};
  if (parser.isSet(theme_option)) {
    QString theme_name = parser.value(theme_option);
    if (theme_name.toLower() == "dark") {
      use_dark_theme = true;

    } else if (theme_name.toLower() == "light") {
      use_dark_theme = false;
    }
  }

  return use_dark_theme;
}

}  // namespace

int main(int argc, char *argv[]) {
  // The PhantomStyle does not really work well on Linux
#ifndef __linux__
  QStyle *phantom_style = new PhantomStyle;
  QStyle *mx_style = new mx::gui::MultiplierStyle(phantom_style);
  QApplication::setStyle(mx_style);
#endif

  QApplication application(argc, argv);
  application.setApplicationName("Multiplier");

  mx::gui::IThemeManager::Initialize(application);

  auto use_dark_theme = ShouldUseDarkTheme(argc, argv);
  mx::gui::IThemeManager::Get().SetTheme(use_dark_theme);

  mx::gui::RegisterMetaTypes();
  mx::gui::InitializeFontDatabase();

  mx::gui::MainWindow main_window;
  main_window.show();

  return application.exec();
}
