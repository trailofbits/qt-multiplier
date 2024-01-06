// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/Interfaces/IInformationExplorerPlugin.h>

namespace mx::gui {

// Implements the call hierarchy plugin, which shows recursive users of entities
// in the reference explorer.
class BuiltinEntityInformationPlugin Q_DECL_FINAL 
    : public IInformationExplorerPlugin {
  Q_OBJECT

 public:
  using IInformationExplorerPlugin::IInformationExplorerPlugin;

  virtual ~BuiltinEntityInformationPlugin(void);

  gap::generator<IInfoGeneratorPtr> CreateInformationCollectors(
      VariantEntity entity) Q_DECL_FINAL;
};

}  // namespace mx::gui
