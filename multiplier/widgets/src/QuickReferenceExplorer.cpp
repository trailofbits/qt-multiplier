// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "QuickReferenceExplorer.h"

#include <multiplier/ui/Util.h>
#include <multiplier/ui/IDatabase.h>
#include <multiplier/ui/Icons.h>

#include <QVBoxLayout>
#include <QKeyEvent>
#include <QApplication>
#include <QPushButton>
#include <QCloseEvent>
#include <QShowEvent>
#include <QLabel>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPoint>

#include <optional>

namespace mx::gui {

struct QuickReferenceExplorer::PrivateData final {
  IReferenceExplorerModel *model{nullptr};
  bool closed{false};
  IReferenceExplorerModel::ExpansionMode expansion_mode;

  std::optional<QPoint> opt_previous_drag_pos;
  QLabel *window_title{nullptr};

  IDatabase::Ptr database;
  QFuture<std::optional<QString>> entity_name_future;
  QFutureWatcher<std::optional<QString>> future_watcher;

  PreviewableReferenceExplorer *reference_explorer{nullptr};
};

QuickReferenceExplorer::QuickReferenceExplorer(
    const Index &index, const FileLocationCache &file_location_cache,
    RawEntityId entity_id,
    const IReferenceExplorerModel::ExpansionMode &expansion_mode,
    const IReferenceExplorer::Mode &mode, QWidget *parent)
    : QWidget(parent),
      d(new PrivateData) {

  d->database = IDatabase::Create(index, file_location_cache);
  connect(&d->future_watcher,
          &QFutureWatcher<QFuture<std::optional<QString>>>::finished, this,
          &QuickReferenceExplorer::EntityNameFutureStatusChanged);

  InitializeWidgets(index, file_location_cache, entity_id, expansion_mode,
                    mode);
}

QuickReferenceExplorer::~QuickReferenceExplorer() {
  CancelRunningRequest();
}

void QuickReferenceExplorer::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    close();

  } else {
    QWidget::keyPressEvent(event);
  }
}

void QuickReferenceExplorer::showEvent(QShowEvent *event) {
  event->accept();
  d->closed = false;
}

void QuickReferenceExplorer::closeEvent(QCloseEvent *event) {
  event->accept();
  d->closed = true;
}

bool QuickReferenceExplorer::eventFilter(QObject *, QEvent *event) {
  if (event->type() == QEvent::MouseButtonPress) {
    auto mouse_event = static_cast<QMouseEvent *>(event);
    OnTitleFrameMousePress(mouse_event);

    return true;

  } else if (event->type() == QEvent::MouseMove) {
    auto mouse_event = static_cast<QMouseEvent *>(event);
    OnTitleFrameMouseMove(mouse_event);

    return true;

  } else if (event->type() == QEvent::MouseButtonRelease) {
    auto mouse_event = static_cast<QMouseEvent *>(event);
    OnTitleFrameMouseRelease(mouse_event);

    return true;
  }

  return false;
}

void QuickReferenceExplorer::InitializeWidgets(
    const Index &index, const FileLocationCache &file_location_cache,
    RawEntityId entity_id,
    const IReferenceExplorerModel::ExpansionMode &expansion_mode,
    const IReferenceExplorer::Mode &mode) {

  setWindowFlags(Qt::Window | Qt::FramelessWindowHint |
                 Qt::WindowStaysOnTopHint);

  setContentsMargins(5, 5, 5, 5);

  connect(qApp, &QGuiApplication::applicationStateChanged, this,
          &QuickReferenceExplorer::OnApplicationStateChange);

  //
  // Title bar
  //

  // Use a temporary window name at first. This won't be shown at all if the
  // name resolution is fast enough
  auto window_name = GenerateWindowName(entity_id, expansion_mode);
  d->window_title = new QLabel(window_name);

  // Start a request to fetch the real entity name
  d->entity_name_future = d->database->RequestEntityName(entity_id);
  d->future_watcher.setFuture(d->entity_name_future);

  // Save as new button
  auto save_to_new_ref_explorer_button = new QPushButton(
      GetIcon(":/Icons/QuickReferenceExplorer/SaveToNewTab"), "", this);

  save_to_new_ref_explorer_button->setToolTip(tr("Save to new tab"));
  save_to_new_ref_explorer_button->setSizePolicy(QSizePolicy::Minimum,
                                                 QSizePolicy::Minimum);

  connect(save_to_new_ref_explorer_button, &QPushButton::clicked, this,
          &QuickReferenceExplorer::OnSaveReferenceExplorer);

  // Close button
  auto close_button = new QPushButton(
      GetIcon(":/Icons/QuickReferenceExplorer/Close"), "", this);

  close_button->setToolTip(tr("Close"));
  close_button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

  connect(close_button, &QPushButton::clicked, this,
          &QuickReferenceExplorer::close);

  // Setup the layout
  auto title_frame_layout = new QHBoxLayout();
  title_frame_layout->setContentsMargins(0, 0, 0, 0);
  title_frame_layout->addWidget(d->window_title);
  title_frame_layout->addStretch();
  title_frame_layout->addWidget(save_to_new_ref_explorer_button);
  title_frame_layout->addWidget(close_button);

  auto title_frame = new QWidget(this);
  title_frame->installEventFilter(this);
  title_frame->setContentsMargins(0, 0, 0, 0);
  title_frame->setLayout(title_frame_layout);

  //
  // Contents
  //

  d->model = IReferenceExplorerModel::Create(index, file_location_cache, this);
  d->model->AppendEntityById(entity_id, expansion_mode, QModelIndex());

  d->reference_explorer = new PreviewableReferenceExplorer(
      index, file_location_cache, d->model, mode, this);

  connect(d->reference_explorer,
          &PreviewableReferenceExplorer::SelectedItemChanged, this,
          &QuickReferenceExplorer::SelectedItemChanged);

  connect(d->reference_explorer, &PreviewableReferenceExplorer::ItemActivated,
          this, &QuickReferenceExplorer::ItemActivated);

  d->reference_explorer->setSizePolicy(QSizePolicy::Expanding,
                                       QSizePolicy::Expanding);

  auto contents_layout = new QVBoxLayout();
  contents_layout->setContentsMargins(0, 0, 0, 0);
  contents_layout->addWidget(d->reference_explorer);

  //
  // Main layout
  //

  auto main_layout = new QVBoxLayout();
  main_layout->setContentsMargins(0, 0, 0, 0);
  main_layout->addWidget(title_frame);
  main_layout->addLayout(contents_layout);

  setLayout(main_layout);
}

void QuickReferenceExplorer::OnTitleFrameMousePress(QMouseEvent *event) {
  d->opt_previous_drag_pos = event->globalPosition().toPoint();
}

void QuickReferenceExplorer::OnTitleFrameMouseMove(QMouseEvent *event) {
  if (!d->opt_previous_drag_pos.has_value()) {
    return;
  }

  auto &previous_drag_pos = d->opt_previous_drag_pos.value();

  auto diff = event->globalPosition().toPoint() - previous_drag_pos;
  previous_drag_pos = event->globalPosition().toPoint();

  move(x() + diff.x(), y() + diff.y());
}

void QuickReferenceExplorer::OnTitleFrameMouseRelease(QMouseEvent *) {
  d->opt_previous_drag_pos = std::nullopt;
}

void QuickReferenceExplorer::OnApplicationStateChange(
    Qt::ApplicationState state) {

  if (d->closed) {
    return;
  }

  auto window_is_visible = state == Qt::ApplicationActive;
  setVisible(window_is_visible);
}

void QuickReferenceExplorer::OnSaveReferenceExplorer() {
  d->model->setParent(d->reference_explorer);

  d->reference_explorer->setWindowTitle(d->window_title->text());

  d->reference_explorer->hide();
  d->reference_explorer->setParent(nullptr);

  layout()->removeWidget(d->reference_explorer);
  emit SaveReferenceExplorer(d->reference_explorer);

  close();
  deleteLater();
}

void QuickReferenceExplorer::EntityNameFutureStatusChanged() {
  if (d->entity_name_future.isCanceled()) {
    return;
  }

  auto opt_entity_name = d->entity_name_future.takeResult();
  if (!opt_entity_name.has_value()) {
    return;
  }

  const auto &entity_name = opt_entity_name.value();

  auto window_name = GenerateWindowName(entity_name, d->expansion_mode);
  d->window_title->setText(window_name);
}

void QuickReferenceExplorer::CancelRunningRequest() {
  if (!d->entity_name_future.isRunning()) {
    return;
  }

  d->entity_name_future.cancel();
  d->entity_name_future.waitForFinished();

  d->entity_name_future = {};
}

QString QuickReferenceExplorer::GenerateWindowName(
    const QString &entity_name, IReferenceExplorerModel::ExpansionMode mode) {

  auto quoted_entity_name = QString("`") + entity_name + "`";

  switch (mode) {
    case IReferenceExplorerModel::CallHierarchyMode:
      return tr("Call hierarchy of ") + quoted_entity_name;

    case IReferenceExplorerModel::TaintMode:
      return tr("Values tainted by ") + quoted_entity_name;
  }
}

QString QuickReferenceExplorer::GenerateWindowName(
    const RawEntityId &entity_id, IReferenceExplorerModel::ExpansionMode mode) {

  auto entity_name = tr("Entity ID #") + QString::number(entity_id);
  return GenerateWindowName(entity_name, mode);
}

}  // namespace mx::gui
