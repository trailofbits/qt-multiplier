// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "MetaTypes.h"

#include <QObject>

#include <multiplier/GUI/Interfaces/IListGenerator.h>
#include <multiplier/Index.h>

namespace mx::gui {

void RegisterMetaTypes(void) {
  qRegisterMetaType<ITreeGeneratorPtr>("ITreeGeneratorPtr");
  qRegisterMetaType<IListGeneratorPtr>("IListGeneratorPtr");
  qRegisterMetaType<DeclCategory>("DeclCategory");

  qRegisterMetaType<uint8_t>("uint8_t");
  qRegisterMetaType<uint16_t>("uint16_t");
  qRegisterMetaType<uint32_t>("uint32_t");
  qRegisterMetaType<uint64_t>("uint64_t");
  qRegisterMetaType<RawEntityId>("RawEntityId");

  qRegisterMetaType<Attr>("Attr");
  qRegisterMetaType<File>("File");
  qRegisterMetaType<Fragment>("Fragment");
  qRegisterMetaType<Decl>("Decl");
  qRegisterMetaType<Stmt>("Stmt");
  qRegisterMetaType<Type>("Type");
  qRegisterMetaType<Token>("Token");
  qRegisterMetaType<Macro>("Macro");

  qRegisterMetaType<std::optional<Attr>>("std::optional<Attr>");
  qRegisterMetaType<std::optional<File>>("std::optional<File>");
  qRegisterMetaType<std::optional<Fragment>>("std::optional<Fragment>");
  qRegisterMetaType<std::optional<Decl>>("std::optional<Decl>");
  qRegisterMetaType<std::optional<Stmt>>("std::optional<Stmt>");
  qRegisterMetaType<std::optional<Type>>("std::optional<Type>");
  qRegisterMetaType<std::optional<Token>>("std::optional<Token>");
  qRegisterMetaType<std::optional<Macro>>("std::optional<Macro>");

  qRegisterMetaType<EntityId>("EntityId");
  qRegisterMetaType<VariantEntity>("VariantEntity");
  qRegisterMetaType<VariantId>("VariantId");
  qRegisterMetaType<FilePathMap>("FilePathMap");
  qRegisterMetaType<Token>("Token");
  qRegisterMetaType<TokenRange>("TokenRange");
  qRegisterMetaType<Index>("Index");
}

}  // namespace mx::gui
