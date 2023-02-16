// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "QuickReferenceExplorer.h"

#include <multiplier/ui/IReferenceExplorer.h>

#include <QVBoxLayout>
#include <QKeyEvent>
#include <QApplication>
#include <QPushButton>

namespace mx::gui {

struct QuickReferenceExplorer::PrivateData final {
  IReferenceExplorerModel *model{nullptr};
  IReferenceExplorer *reference_explorer{nullptr};
  QPushButton *save_all_button{nullptr};
};

QuickReferenceExplorer::QuickReferenceExplorer(
    mx::Index index, mx::FileLocationCache file_location_cache,
    RawEntityId entity_id, QWidget *parent)
    : QWidget(parent),
      d(new PrivateData) {

  setWindowFlags(Qt::Window | Qt::FramelessWindowHint |
                 Qt::WindowStaysOnTopHint);

  InitializeWidgets(index, file_location_cache, entity_id);
}

QuickReferenceExplorer::~QuickReferenceExplorer() {}

void QuickReferenceExplorer::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    close();

  } else {
    QWidget::keyPressEvent(event);
  }
}

void QuickReferenceExplorer::resizeEvent(QResizeEvent *event) {
  UpdateSaveAllButtonPosition();
  QWidget::resizeEvent(event);
}

void QuickReferenceExplorer::InitializeWidgets(
    mx::Index index, mx::FileLocationCache file_location_cache,
    RawEntityId entity_id) {

  setContentsMargins(0, 0, 0, 0);

  d->model = IReferenceExplorerModel::Create(index, file_location_cache, this);
  d->model->AppendEntityObject(entity_id, QModelIndex());

  d->reference_explorer = IReferenceExplorer::Create(d->model);

  auto layout = new QVBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(d->reference_explorer);
  setLayout(layout);

  connect(qApp, &QGuiApplication::applicationStateChanged, this,
          &QuickReferenceExplorer::OnApplicationStateChange);

  d->save_all_button = new QPushButton(
      QIcon(":/Icons/QuickReferenceExplorer/SaveAll"), "", this);

  d->save_all_button->resize(20, 20);
  UpdateSaveAllButtonPosition();

  connect(d->save_all_button, &QPushButton::clicked, this,
          &QuickReferenceExplorer::OnSaveAllButtonPress);
}

void QuickReferenceExplorer::UpdateSaveAllButtonPosition() {
  auto button_width = d->save_all_button->width();

  auto x = width() - button_width;
  d->save_all_button->move(x, 0);
  d->save_all_button->raise();
}

void QuickReferenceExplorer::OnApplicationStateChange(
    Qt::ApplicationState state) {

  auto window_is_visible = state == Qt::ApplicationActive;
  setVisible(window_is_visible);
}

void QuickReferenceExplorer::OnSaveAllButtonPress() {
  auto mime_data = d->model->mimeData({QModelIndex()});
  mime_data->setParent(this);

  emit SaveAll(mime_data);
  close();
}

}  // namespace mx::gui
