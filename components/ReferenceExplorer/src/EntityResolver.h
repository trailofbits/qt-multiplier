// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QObject>
#include <QRunnable>

#include <memory>
#include <multiplier/Entity.h>
#include <multiplier/ui/IReferenceExplorerModel.h>

namespace mx {
class Index;
}  // namespace mx
namespace mx::gui {

//! Implements the IEntityNameResolver interface
class EntityResolver final : public QObject, public QRunnable {
  Q_OBJECT

 public:

  //! Constructor
  EntityResolver(const Index &index, const RawEntityId &entity_id,
                 IReferenceExplorerModel::ExpansionMode mode,
                 const QModelIndex &location);

  //! Destructor
  virtual ~EntityResolver() override;

  //! \copybrief QRunnable::run
  virtual void run() override;

 signals:
  void Finished(VariantEntity, IReferenceExplorerModel::ExpansionMode mode,
                const QModelIndex &location);

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;
};

}  // namespace mx::gui
