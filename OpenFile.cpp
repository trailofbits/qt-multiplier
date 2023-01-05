// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <cstdlib>
#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDebug>
#include <multiplier/Index.h>
#include <multiplier/ui/ICodeModel.h>
#include <multiplier/ui/ICodeView.h>

#include "Multiplier.h"

int main(int argc, char *argv[]) {

  mx::gui::Multiplier multiplier(argc, argv);
  multiplier.setApplicationName("Multiplier: Open a file");

  QCommandLineParser parser;
  QCommandLineOption db_option("db");
  db_option.setValueName("db");
  parser.addOption(db_option);

  QCommandLineOption file_id_option("file_id");
  file_id_option.setValueName("file_id");
  parser.addOption(file_id_option);

  parser.process(multiplier);
  
  if (!parser.isSet(db_option)) {
    qDebug() << "Missing option '--db'.";
    return EXIT_FAILURE;
  }

  if (!parser.isSet(file_id_option)) {
    qDebug() << "Missing option '--file_id'.";
    return EXIT_FAILURE;
  }

  bool converted = false;
  mx::RawEntityId raw_file_id =
      parser.value(file_id_option).toULongLong(&converted);

  if (!converted) {
    qDebug() << "Invalid or non-numerical value passed to '--file_id'.";
    return EXIT_FAILURE;
  }

  mx::VariantId vid = mx::EntityId(raw_file_id).Unpack();
  if (!std::holds_alternative<mx::FileId>(vid)) {
    qDebug() << "Value passed to '--file_id' is not formatted as a "
                "Multiplier file id.";
    return EXIT_FAILURE;
  }

  mx::FileId file_id = std::get<mx::FileId>(vid);

  mx::Index index(mx::EntityProvider::in_memory_cache(
      mx::EntityProvider::from_database(
          parser.value(db_option).toStdString())));

  mx::FileLocationCache file_loc_cache;

  auto model = mx::gui::ICodeModel::Create(file_loc_cache, index);
  auto view = mx::gui::ICodeView::Create(model);
  view->setTheme(mx::gui::GetDefaultTheme(true));
  model->SetFile(file_id);

  return multiplier.Run(view);
}
