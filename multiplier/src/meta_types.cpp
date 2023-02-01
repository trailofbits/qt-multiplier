// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "meta_types.h"

#include <QObject>

#include <multiplier/Index.h>

namespace mx::gui {

void RegisterMetaTypes() {
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
  qRegisterMetaType<std::vector<mx::PackedFragmentId>>(
      "std::vector<PackedFragmentId>");
  qRegisterMetaType<std::vector<mx::PackedFileId>>("std::vector<PackedFileId>");
  qRegisterMetaType<std::optional<mx::PackedFileId>>(
      "std::optional<PackedFileId>");
}

}  // namespace mx::gui
