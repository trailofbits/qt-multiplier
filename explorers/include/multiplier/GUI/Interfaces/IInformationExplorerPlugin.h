// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QObject>

#include <multiplier/Index.h>

#include "IInfoGenerator.h"

namespace mx {
class FileLocationCache;
}  // namespace mx
namespace mx::gui {

//! Interface that information explorer plugins must follow.
class IInformationExplorerPlugin : public QObject {
  Q_OBJECT

 public:
  virtual ~IInformationExplorerPlugin(void);

  virtual gap::generator<IInfoGeneratorPtr> CreateInformationCollectors(
      FileLocationCache file_location_cache,
      VariantEntity entity) = 0;
};

}  // namespace mx::gui

Q_DECLARE_INTERFACE(mx::gui::IInformationExplorerPlugin,
                    "com.trailofbits.interface.IInformationExplorerPlugin")
