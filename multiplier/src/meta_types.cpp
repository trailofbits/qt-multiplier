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
  qRegisterMetaType<NamedEntityList>("NamedEntityList");
  qRegisterMetaType<DeclCategory>("DeclCategory");
  qRegisterMetaType<uint8_t>("uint8_t");
  qRegisterMetaType<uint16_t>("uint16_t");
  qRegisterMetaType<uint32_t>("uint32_t");
  qRegisterMetaType<uint64_t>("uint64_t");
  qRegisterMetaType<std::optional<Attr>>("std::optional<Attr>");
  qRegisterMetaType<std::optional<File>>("std::optional<File>");
  qRegisterMetaType<std::optional<Fragment>>("std::optional<Fragment>");
  qRegisterMetaType<std::optional<Decl>>("std::optional<Decl>");
  qRegisterMetaType<std::optional<Stmt>>("std::optional<Stmt>");
  qRegisterMetaType<std::optional<Type>>("std::optional<Type>");
  qRegisterMetaType<std::optional<Token>>("std::optional<Token>");
  qRegisterMetaType<std::optional<Macro>>("std::optional<Macro>");
  qRegisterMetaType<RawEntityId>("RawEntityId");
  qRegisterMetaType<EntityId>("EntityId");
  qRegisterMetaType<VariantEntity>("VariantEntity");
  qRegisterMetaType<VariantId>("VariantEntity");
  qRegisterMetaType<FilePathMap>("FilePathMap");
  qRegisterMetaType<Token>("Token");
  qRegisterMetaType<TokenRange>("TokenRange");
  qRegisterMetaType<Index>("Index");
  qRegisterMetaType<Index>("::mx::Index");
  qRegisterMetaType<Index>("mx::Index");
  qRegisterMetaType<std::vector<Fragment>>("std::vector<Fragment>");
  qRegisterMetaType<std::vector<RawEntityId>>("std::vector<RawEntityId>");
  qRegisterMetaType<FragmentIdList>("FragmentIdList");
  qRegisterMetaType<std::vector<PackedFragmentId>>(
      "std::vector<PackedFragmentId>");
  qRegisterMetaType<std::vector<PackedFileId>>("std::vector<PackedFileId>");
  qRegisterMetaType<std::optional<PackedFileId>>(
      "std::optional<PackedFileId>");
}

}  // namespace mx::gui
