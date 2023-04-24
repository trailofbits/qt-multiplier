// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "QuickCodeView.h"
#include "CodePreviewModelAdapter.h"
#include <multiplier/ui/IDatabase.h>

#include <QVBoxLayout>
#include <QLabel>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QEvent>
#include <QMouseEvent>
#include <QApplication>
#include <QPushButton>

#include <optional>

namespace mx::gui {

struct QuickCodeView::PrivateData final {
  bool closed{false};

  std::optional<QPoint> opt_previous_drag_pos;
  QLabel *window_title{nullptr};

  IDatabase::Ptr database;
  QFuture<std::optional<QString>> entity_name_future;
  QFutureWatcher<std::optional<QString>> future_watcher;
};

QuickCodeView::QuickCodeView(const Index &index,
                             const FileLocationCache &file_location_cache,
                             RawEntityId entity_id, QWidget *parent)
    : QWidget(parent),
      d(new PrivateData) {

  d->database = IDatabase::Create(index, file_location_cache);
  connect(&d->future_watcher,
          &QFutureWatcher<QFuture<std::optional<QString>>>::finished, this,
          &QuickCodeView::EntityNameFutureStatusChanged);

  InitializeWidgets(index, file_location_cache, entity_id);
}

QuickCodeView::~QuickCodeView() {}

void QuickCodeView::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    close();

  } else {
    QWidget::keyPressEvent(event);
  }
}

void QuickCodeView::showEvent(QShowEvent *event) {
  event->accept();
  d->closed = false;
}

void QuickCodeView::closeEvent(QCloseEvent *event) {
  event->accept();
  d->closed = true;
}

bool QuickCodeView::eventFilter(QObject *, QEvent *event) {
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

void QuickCodeView::InitializeWidgets(
    const Index &index, const FileLocationCache &file_location_cache,
    RawEntityId entity_id) {

  setWindowFlags(Qt::Window | Qt::FramelessWindowHint |
                 Qt::WindowStaysOnTopHint);

  setContentsMargins(5, 5, 5, 5);

  connect(qApp, &QGuiApplication::applicationStateChanged, this,
          &QuickCodeView::OnApplicationStateChange);

  //
  // Title bar
  //

  // Use a temporary window name at first. This won't be shown at all if the
  // name resolution is fast enough
  auto window_name = tr("Entity ID #") + QString::number(entity_id);
  d->window_title = new QLabel(window_name);

  // Start a request to fetch the real entity name
  d->entity_name_future = d->database->RequestEntityName(entity_id);
  d->future_watcher.setFuture(d->entity_name_future);

  // Close button
  auto close_button =
      new QPushButton(QIcon(":/Icons/QuickCodeView/Close"), "", this);

  close_button->setToolTip(tr("Close"));
  close_button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

  connect(close_button, &QPushButton::clicked, this, &QuickCodeView::close);

  // Setup the layout
  auto title_frame_layout = new QHBoxLayout();
  title_frame_layout->setContentsMargins(0, 0, 0, 0);
  title_frame_layout->addWidget(d->window_title);
  title_frame_layout->addStretch();
  title_frame_layout->addWidget(close_button);

  auto title_frame = new QWidget(this);
  title_frame->installEventFilter(this);
  title_frame->setContentsMargins(0, 0, 0, 0);
  title_frame->setLayout(title_frame_layout);

  //
  // Contents
  //

  auto main_model = ICodeModel::Create(file_location_cache, index, this);
  auto proxy_model = new CodePreviewModelAdapter(main_model, this);
  auto view = ICodeView::Create(proxy_model, this);
  view->SetWordWrapping(true);

  connect(view, &ICodeView::TokenTriggered, this,
          &QuickCodeView::OnTokenTriggered);

  proxy_model->SetEntity(entity_id);

  auto contents_layout = new QVBoxLayout();
  contents_layout->setContentsMargins(0, 0, 0, 0);
  contents_layout->addWidget(view);

  //
  // Main layout
  //

  auto main_layout = new QVBoxLayout();
  main_layout->setContentsMargins(0, 0, 0, 0);
  main_layout->addWidget(title_frame);
  main_layout->addLayout(contents_layout);

  setLayout(main_layout);
}

void QuickCodeView::OnTitleFrameMousePress(QMouseEvent *event) {
  d->opt_previous_drag_pos = event->globalPosition().toPoint();
}

void QuickCodeView::OnTitleFrameMouseMove(QMouseEvent *event) {
  if (!d->opt_previous_drag_pos.has_value()) {
    return;
  }

  auto &previous_drag_pos = d->opt_previous_drag_pos.value();

  auto diff = event->globalPosition().toPoint() - previous_drag_pos;
  previous_drag_pos = event->globalPosition().toPoint();

  move(x() + diff.x(), y() + diff.y());
}

void QuickCodeView::OnTitleFrameMouseRelease(QMouseEvent *) {
  d->opt_previous_drag_pos = std::nullopt;
}

void QuickCodeView::OnApplicationStateChange(Qt::ApplicationState state) {

  if (d->closed) {
    return;
  }

  auto window_is_visible = state == Qt::ApplicationActive;
  setVisible(window_is_visible);
}

void QuickCodeView::EntityNameFutureStatusChanged() {
  if (d->entity_name_future.isCanceled()) {
    return;
  }

  auto opt_entity_name = d->entity_name_future.takeResult();
  if (!opt_entity_name.has_value()) {
    return;
  }

  const auto &entity_name = opt_entity_name.value();

  auto window_name = tr("Preview for") + " `" + entity_name + "`";
  d->window_title->setText(window_name);
}

void QuickCodeView::OnTokenTriggered(const ICodeView::TokenAction &token_action,
                                     const CodeModelIndex &index) {

  switch (token_action.type) {
    case ICodeView::TokenAction::Type::Keyboard:
    case ICodeView::TokenAction::Type::Primary:
    case ICodeView::TokenAction::Type::Secondary:
      emit TokenTriggered(token_action, index);
      break;

    case ICodeView::TokenAction::Type::Hover: break;
  }
}

}  // namespace mx::gui
