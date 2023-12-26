// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "MainWindow.h"
#include "MetaTypes.h"
#include "Style.h"

#include <multiplier/Index.h>
#include <multiplier/ui/Context.h>
#include <multiplier/ui/FontDatabase.h>
#include <multiplier/ui/IThemeManager.h>

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFileDialog>
#include <QProxyStyle>

#include <phantom/phantomstyle.h>

namespace {

static bool kDefaultToDarkTheme = true;

static bool ShouldUseDarkTheme(const QCommandLineParser &parser,
                               const QCommandLineOption &theme_option) {

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

static mx::Index OpenDatabase(const QCommandLineParser &parser,
                          const QCommandLineOption &db_option) {

  QString db_path;
  if (!parser.isSet(db_option)) {
    db_path = QFileDialog::getOpenFileName(
        nullptr, QObject::tr("Select a Multiplier database"), QDir::homePath());
  } else {
    db_path = parser.value(db_option);
  }

  return mx::Index::in_memory_cache(
      mx::Index::from_database(db_path.toStdString()));
}

}  // namespace

int main(int argc, char *argv[]) {
  QCommandLineOption theme_option("theme");
  theme_option.setValueName("theme");

  QCommandLineOption db_option("database");
  db_option.setValueName("database");

  QCommandLineParser parser;
  parser.addOption(theme_option);
  parser.addOption(db_option);

  // The PhantomStyle does not really work well on Linux
#ifndef __linux__
  QStyle *phantom_style = new PhantomStyle;
  QStyle *mx_style = new mx::gui::MultiplierStyle(phantom_style);
  QApplication::setStyle(mx_style);
#endif

  QApplication application(argc, argv);
  application.setApplicationName("Multiplier");

  parser.process(application);

  mx::gui::IThemeManager::Initialize(application);
  mx::gui::RegisterMetaTypes();
  mx::gui::InitializeFontDatabase();

  mx::gui::Context context(OpenDatabase(parser, db_option));
  context.ThemeManager().SetTheme(ShouldUseDarkTheme(parser, theme_option));

  mx::gui::MainWindow main_window(context);
  main_window.show();

  return application.exec();
}
