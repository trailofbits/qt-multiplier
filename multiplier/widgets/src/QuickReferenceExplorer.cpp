// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "QuickReferenceExplorer.h"
#include "PreviewableReferenceExplorer.h"

#include <multiplier/ui/Util.h>
#include <multiplier/ui/IEntityNameResolver.h>

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
#include <QThread>

#include <optional>

namespace mx::gui {

struct QuickReferenceExplorer::PrivateData final {
  IReferenceExplorerModel *model{nullptr};
  bool closed{false};

  std::optional<QPoint> opt_previous_drag_pos;
  QLabel *window_title{nullptr};

  QThread *name_resolver_thread{nullptr};
  IEntityNameResolver *entity_name_resolver{nullptr};
};

QuickReferenceExplorer::QuickReferenceExplorer(
    mx::Index index, mx::FileLocationCache file_location_cache,
    RawEntityId entity_id, QWidget *parent)
    : QWidget(parent),
      d(new PrivateData) {

  InitializeWidgets(index, file_location_cache, entity_id);
}

QuickReferenceExplorer::~QuickReferenceExplorer() {
  // Ask the EntityNameResolver instance to stop
  d->name_resolver_thread->requestInterruption();

  // Terminate the thread
  d->name_resolver_thread->quit();
  d->name_resolver_thread->wait();
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
    mx::Index index, mx::FileLocationCache file_location_cache,
    RawEntityId entity_id) {

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
  auto window_name = tr("References to entity ") + QString::number(entity_id);
  d->window_title = new QLabel(window_name);

  // Start a request to fetch the real entity name
  d->name_resolver_thread = new QThread(this);

  d->entity_name_resolver = IEntityNameResolver::Create(index, entity_id);
  d->entity_name_resolver->moveToThread(d->name_resolver_thread);

  connect(d->name_resolver_thread, &QThread::started, d->entity_name_resolver,
          &IEntityNameResolver::Start);

  connect(d->entity_name_resolver, &IEntityNameResolver::Finished, this,
          &QuickReferenceExplorer::OnEntityNameResolutionFinished);

  connect(d->entity_name_resolver, &IEntityNameResolver::Finished,
          d->name_resolver_thread, &QThread::quit);

  d->name_resolver_thread->start();

  // Save to active button
  auto save_to_active_ref_explorer_button = new QPushButton(
      QIcon(":/Icons/QuickReferenceExplorer/SaveToActiveTab"), "", this);

  save_to_active_ref_explorer_button->setToolTip(tr("Save to active tab"));
  save_to_active_ref_explorer_button->setSizePolicy(QSizePolicy::Minimum,
                                                    QSizePolicy::Minimum);

  connect(save_to_active_ref_explorer_button, &QPushButton::clicked, this,
          &QuickReferenceExplorer::OnSaveAllToActiveRefExplorerButtonPress);

  // Save as new button
  auto save_to_new_ref_explorer_button = new QPushButton(
      QIcon(":/Icons/QuickReferenceExplorer/SaveToNewTab"), "", this);

  save_to_new_ref_explorer_button->setToolTip(tr("Save to new tab"));
  save_to_new_ref_explorer_button->setSizePolicy(QSizePolicy::Minimum,
                                                 QSizePolicy::Minimum);

  connect(save_to_new_ref_explorer_button, &QPushButton::clicked, this,
          &QuickReferenceExplorer::OnSaveAllToNewRefExplorerButtonPress);

  // Close button
  auto close_button =
      new QPushButton(QIcon(":/Icons/QuickReferenceExplorer/Close"), "", this);

  close_button->setToolTip(tr("Close"));
  close_button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

  connect(close_button, &QPushButton::clicked, this,
          &QuickReferenceExplorer::close);

  // Setup the layout
  auto title_frame_layout = new QHBoxLayout();
  title_frame_layout->setContentsMargins(0, 0, 0, 0);
  title_frame_layout->addWidget(d->window_title);
  title_frame_layout->addStretch();
  title_frame_layout->addWidget(save_to_active_ref_explorer_button);
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
  d->model->AppendEntityObject(entity_id, QModelIndex());

  auto reference_explorer = new PreviewableReferenceExplorer(
      index, file_location_cache, d->model, this);

  connect(reference_explorer, &PreviewableReferenceExplorer::ItemClicked, this,
          &QuickReferenceExplorer::ItemClicked);

  reference_explorer->setSizePolicy(QSizePolicy::Expanding,
                                    QSizePolicy::Expanding);

  auto contents_layout = new QVBoxLayout();
  contents_layout->setContentsMargins(0, 0, 0, 0);
  contents_layout->addWidget(reference_explorer);

  //
  // Main layout
  //

  auto main_layout = new QVBoxLayout();
  main_layout->setContentsMargins(0, 0, 0, 0);
  main_layout->addWidget(title_frame);
  main_layout->addLayout(contents_layout);

  setLayout(main_layout);
}

void QuickReferenceExplorer::EmitSaveSignal(const bool &as_new_tab) {
  auto mime_data = d->model->mimeData({QModelIndex()});
  mime_data->setParent(this);

  emit SaveAll(mime_data, d->window_title->text(), as_new_tab);
  close();
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

void QuickReferenceExplorer::OnSaveAllToActiveRefExplorerButtonPress() {
  EmitSaveSignal(false);
}

void QuickReferenceExplorer::OnSaveAllToNewRefExplorerButtonPress() {
  EmitSaveSignal(true);
}

void QuickReferenceExplorer::OnEntityNameResolutionFinished(
    const std::optional<QString> &opt_entity_name) {

  if (!opt_entity_name.has_value()) {
    return;
  }

  const auto &entity_name = opt_entity_name.value();

  auto title = tr("References to ") + "`" + entity_name + "`";
  d->window_title->setText(title);
}

}  // namespace mx::gui
