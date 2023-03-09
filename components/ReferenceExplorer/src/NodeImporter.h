/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/IReferenceExplorerModel.h>
#include <QObject>
#include <QRunnable>

#include "Types.h"

namespace mx {
class FileLocationCache;
namespace gui {

//! A worker that can be used to import model nodes asynchronously. Model nodes
//! are associated with two entities: the referenced entity, which corresponds
//! to its general "location", and the primary entity, which corresponds to
//! its name. In many cases, the referenced and primary entities will match,
//! though in others, such as with the call hierarchy nodes, the referenced node
//! will be something inside of a function, and the primary entity will be the
//! function itself. The primary entity is the subject of expansion.
class NodeImporter final : public QObject, public QRunnable {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  virtual ~NodeImporter(void);

  static Node CreateNode(const FileLocationCache &file_cache,
                         VariantEntity entity, VariantEntity referenced_entity,
                         IReferenceExplorerModel::ExpansionMode import_mode);

  NodeImporter(
      const FileLocationCache &file_cache,
      VariantEntity entity, VariantEntity referenced_entity,
      IReferenceExplorerModel::ExpansionMode import_mode,
      const QModelIndex &parent);

  void run() final;

 signals:
  void Finished(Node node, const QModelIndex &parent);
};

}  // namespace gui
}  // namespace mx
