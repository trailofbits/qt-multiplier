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
#include <QCloseEvent>
#include <QShowEvent>

namespace mx::gui {

struct QuickReferenceExplorer::PrivateData final {
  IReferenceExplorerModel *model{nullptr};
  IReferenceExplorer *reference_explorer{nullptr};
  QPushButton *save_to_active_ref_explorer_button{nullptr};
  QPushButton *save_to_new_ref_explorer_button{nullptr};
  bool closed{false};
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
  UpdateButtonPositions();
  QWidget::resizeEvent(event);
}

void QuickReferenceExplorer::showEvent(QShowEvent *event) {
  event->accept();
  d->closed = false;
}

void QuickReferenceExplorer::closeEvent(QCloseEvent *event) {
  event->accept();
  d->closed = true;
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

  d->save_to_active_ref_explorer_button = new QPushButton(
      QIcon(":/Icons/QuickReferenceExplorer/SaveToActiveTab"), "", this);

  d->save_to_active_ref_explorer_button->setToolTip(tr("Save to active tab"));

  d->save_to_active_ref_explorer_button->resize(20, 20);

  d->save_to_new_ref_explorer_button = new QPushButton(
      QIcon(":/Icons/QuickReferenceExplorer/SaveToNewTab"), "", this);

  d->save_to_new_ref_explorer_button->setToolTip(tr("Save to new tab"));

  d->save_to_new_ref_explorer_button->resize(20, 20);

  UpdateButtonPositions();

  connect(d->save_to_active_ref_explorer_button, &QPushButton::clicked, this,
          &QuickReferenceExplorer::OnSaveAllToActiveRefExplorerButtonPress);

  connect(d->save_to_new_ref_explorer_button, &QPushButton::clicked, this,
          &QuickReferenceExplorer::OnSaveAllToNewRefExplorerButtonPress);
}

void QuickReferenceExplorer::UpdateButtonPositions() {
  auto button_width = d->save_to_active_ref_explorer_button->width();

  auto x = width() - button_width;
  d->save_to_active_ref_explorer_button->move(x, 0);
  d->save_to_active_ref_explorer_button->raise();

  x -= button_width;
  d->save_to_new_ref_explorer_button->move(x, 0);
  d->save_to_new_ref_explorer_button->raise();
}

void QuickReferenceExplorer::EmitSaveSignal(const bool &as_new_tab) {
  auto mime_data = d->model->mimeData({QModelIndex()});
  mime_data->setParent(this);

  emit SaveAll(mime_data, as_new_tab);
  close();
}

void QuickReferenceExplorer::OnApplicationStateChange(
    Qt::ApplicationState state) {

  if (d->closed) {
    return;
  }

  auto window_is_visible = state == Qt::ApplicationActive;
  setVisible(window_is_visible);
}

void QuickReferenceExplorer::OnSaveAllToActiveRefExplorerButtonPress() {
  EmitSaveSignal(false);
}

void QuickReferenceExplorer::OnSaveAllToNewRefExplorerButtonPress() {
  EmitSaveSignal(true);
}

}  // namespace mx::gui
