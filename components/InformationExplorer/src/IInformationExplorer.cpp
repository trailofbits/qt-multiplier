/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "InformationExplorer.h"

#include <multiplier/ui/IInformationExplorer.h>

namespace mx::gui {

IInformationExplorer *
IInformationExplorer::Create(IInformationExplorerModel *model,
                             QWidget *parent) {
  return new InformationExplorer(model, parent);
}

}  // namespace mx::gui
