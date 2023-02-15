// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "QuickReferenceExplorer.h"

#include <multiplier/ui/IReferenceExplorer.h>

#include <QVBoxLayout>

namespace mx::gui {

struct QuickReferenceExplorer::PrivateData final {
  IReferenceExplorerModel *model{nullptr};
  IReferenceExplorer *reference_explorer{nullptr};
};

QuickReferenceExplorer::QuickReferenceExplorer(
    mx::Index index, mx::FileLocationCache file_location_cache,
    RawEntityId entity_id, QWidget *parent)
    : QWidget(parent),
      d(new PrivateData) {

  setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
  InitializeWidgets(index, file_location_cache, entity_id);
}

QuickReferenceExplorer::~QuickReferenceExplorer() {}

void QuickReferenceExplorer::leaveEvent(QEvent *) {
  close();
}

void QuickReferenceExplorer::InitializeWidgets(
    mx::Index index, mx::FileLocationCache file_location_cache,
    RawEntityId entity_id) {

  setContentsMargins(0, 0, 0, 0);
  setAttribute(Qt::WA_DeleteOnClose);

  d->model = IReferenceExplorerModel::Create(index, file_location_cache, this);
  d->model->AppendEntityObject(
      entity_id, IReferenceExplorerModel::EntityObjectType::CallHierarchy,
      QModelIndex());

  d->reference_explorer = IReferenceExplorer::Create(d->model);

  auto layout = new QVBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(d->reference_explorer);
  setLayout(layout);
}

}  // namespace mx::gui
