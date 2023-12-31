// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "MetaTypes.h"
#include "MultiplierStyle.h"

#include <multiplier/Index.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/MediaManager.h>
#include <multiplier/GUI/Managers/ThemeManager.h>
#include <multiplier/GUI/Themes/BuiltinTheme.h>

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFileDialog>
#include <QProxyStyle>

#include <phantom/phantomstyle.h>

namespace {

// static mx::Index OpenDatabase(const QCommandLineParser &parser,
//                               const QCommandLineOption &db_option) {

//   QString db_path;
//   if (!parser.isSet(db_option)) {
//     db_path = QFileDialog::getOpenFileName(
//         nullptr, QObject::tr("Select a Multiplier database"), QDir::homePath());
//   } else {
//     db_path = parser.value(db_option);
//   }

//   return mx::Index::in_memory_cache(
//       mx::Index::from_database(db_path.toStdString()));
// }

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
  application.setOrganizationName("Trail of Bits");
  application.setOrganizationDomain("trailofbits.com");
  application.setApplicationName("Multiplier");

  mx::gui::RegisterMetaTypes();

  mx::gui::ConfigManager config(application);
  mx::gui::ThemeManager &theme_manager = config.ThemeManager();
  mx::gui::MediaManager &media_manager = config.MediaManager();

  theme_manager.Register(mx::gui::CreateDarkTheme(media_manager));
  theme_manager.Register(mx::gui::CreateLightTheme(media_manager));

  parser.process(application);

  // Set the theme.
  if (parser.isSet(theme_option)) {
    if (auto theme = theme_manager.Find(parser.value(theme_option))) {
      theme_manager.SetTheme(std::move(theme));
    }
  }

  // mx::gui::Context context(OpenDatabase(parser, db_option));
  // context.ThemeManager().SetTheme(ShouldUseDarkTheme(parser, theme_option));

  // mx::gui::MainWindow main_window(context);
  // main_window.show();

  return application.exec();
}
