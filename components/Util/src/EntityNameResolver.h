// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/ui/IEntityNameResolver.h>

#include <memory>

namespace mx::gui {

//! Implements the IEntityNameResolver interface
class EntityNameResolver final : public IEntityNameResolver {
  Q_OBJECT

 public:
  //! Destructor
  virtual ~EntityNameResolver() override;

 public slots:
  //! \copybrief IEntityNameResolver::run
  virtual void run() override;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Constructor
  EntityNameResolver(Index index, const RawEntityId &entity_id);

  friend class IEntityNameResolver;
};

}  // namespace mx::gui
