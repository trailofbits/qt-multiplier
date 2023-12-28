/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/GUI/IDatabase.h>

#include <multiplier/Index.h>

#include <QPromise>
#include <QString>

namespace mx::gui {

void GetEntityList(QPromise<bool> &result_promise, const Index &index,
                   IDatabase::QueryEntitiesReceiver *receiver,
                   const QString &string,
                   const IDatabase::QueryEntitiesMode &query_mode);

}  // namespace mx::gui
