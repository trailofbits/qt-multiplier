// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "Multiplier.h"

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

extern int QT_MANGLE_NAMESPACE(qInitResources_resources)();

namespace mx {
namespace gui {
namespace {

static struct Init {
  Init(void) {
    qInitResources_resources();
    QApplication::setStyle("Fusion");
  }
} gInitializer;

}  // namespace

Multiplier::Multiplier(int &argc, char *argv[])
    : QApplication(argc, argv),
      splash_screen(QPixmap(":/Icons/appicon")) {

  splash_screen.show();
  processEvents();

#define ADD_FONT(path) \
    do { \
      auto ret = QFontDatabase::addApplicationFont(path); \
      assert(ret != -1); \
      std::ignore = ret; \
    } while (0)

  ADD_FONT(":/Fonts/Fonts/Source_Code_Pro/static/SourceCodePro-Black.ttf");
  ADD_FONT(":/Fonts/Fonts/Source_Code_Pro/static/SourceCodePro-BlackItalic.ttf");
  ADD_FONT(":/Fonts/Fonts/Source_Code_Pro/static/SourceCodePro-Bold.ttf");
  ADD_FONT(":/Fonts/Fonts/Source_Code_Pro/static/SourceCodePro-BoldItalic.ttf");
  ADD_FONT(":/Fonts/Fonts/Source_Code_Pro/static/SourceCodePro-ExtraBold.ttf");
  ADD_FONT(":/Fonts/Fonts/Source_Code_Pro/static/SourceCodePro-ExtraBoldItalic.ttf");
  ADD_FONT(":/Fonts/Fonts/Source_Code_Pro/static/SourceCodePro-ExtraLight.ttf");
  ADD_FONT(":/Fonts/Fonts/Source_Code_Pro/static/SourceCodePro-ExtraLightItalic.ttf");
  ADD_FONT(":/Fonts/Fonts/Source_Code_Pro/static/SourceCodePro-Italic.ttf");
  ADD_FONT(":/Fonts/Fonts/Source_Code_Pro/static/SourceCodePro-Light.ttf");
  ADD_FONT(":/Fonts/Fonts/Source_Code_Pro/static/SourceCodePro-LightItalic.ttf");
  ADD_FONT(":/Fonts/Fonts/Source_Code_Pro/static/SourceCodePro-Medium.ttf");
  ADD_FONT(":/Fonts/Fonts/Source_Code_Pro/static/SourceCodePro-MediumItalic.ttf");
  ADD_FONT(":/Fonts/Fonts/Source_Code_Pro/static/SourceCodePro-Regular.ttf");
  ADD_FONT(":/Fonts/Fonts/Source_Code_Pro/static/SourceCodePro-SemiBold.ttf");
  ADD_FONT(":/Fonts/Fonts/Source_Code_Pro/static/SourceCodePro-SemiBoldItalic.ttf");

  qRegisterMetaType<mx::NamedEntityList>("NamedEntityList");
  qRegisterMetaType<mx::DeclCategory>("DeclCategory");
  qRegisterMetaType<uint8_t>("uint8_t");
  qRegisterMetaType<uint16_t>("uint16_t");
  qRegisterMetaType<uint32_t>("uint32_t");
  qRegisterMetaType<uint64_t>("uint64_t");
  qRegisterMetaType<std::optional<mx::Attr>>("std::optional<Attr>");
  qRegisterMetaType<std::optional<mx::File>>("std::optional<File>");
  qRegisterMetaType<std::optional<mx::Fragment>>("std::optional<Fragment>");
  qRegisterMetaType<std::optional<mx::Decl>>("std::optional<Decl>");
  qRegisterMetaType<std::optional<mx::Stmt>>("std::optional<Stmt>");
  qRegisterMetaType<std::optional<mx::Type>>("std::optional<Type>");
  qRegisterMetaType<std::optional<mx::Token>>("std::optional<Token>");
  qRegisterMetaType<std::optional<mx::Macro>>("std::optional<Macro>");
  qRegisterMetaType<mx::RawEntityId>("RawEntityId");
  qRegisterMetaType<mx::EntityId>("EntityId");
  qRegisterMetaType<mx::VariantEntity>("VariantEntity");
  qRegisterMetaType<mx::VariantId>("VariantEntity");
  qRegisterMetaType<mx::FilePathMap>("FilePathMap");
  qRegisterMetaType<mx::Token>("Token");
  qRegisterMetaType<mx::TokenRange>("TokenRange");
  qRegisterMetaType<mx::Index>("Index");
  qRegisterMetaType<mx::Index>("::mx::Index");
  qRegisterMetaType<mx::Index>("mx::Index");
  qRegisterMetaType<std::vector<mx::Fragment>>("std::vector<Fragment>");
  qRegisterMetaType<std::vector<mx::RawEntityId>>("std::vector<RawEntityId>");
  qRegisterMetaType<mx::FragmentIdList>("FragmentIdList");
  qRegisterMetaType<std::vector<mx::PackedFragmentId>>("std::vector<PackedFragmentId>");
  qRegisterMetaType<std::vector<mx::PackedFileId>>("std::vector<PackedFileId>");
}

int Multiplier::Run(QWidget *main_window) {
  main_window->show();
  splash_screen.finish(main_window);
  return exec();
}

}  // namespace gui
}  // namespace mx
