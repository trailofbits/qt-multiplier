// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "QuickReferenceExplorer.h"

#include <multiplier/ui/Util.h>
#include <multiplier/ui/IDatabase.h>
#include <multiplier/ui/Icons.h>
#include <multiplier/ui/IThemeManager.h>
#include <multiplier/ui/IGeneratorModel.h>
#include <multiplier/ui/ITreeGenerator.h>

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
#include <QSizeGrip>
#include <QGridLayout>

#include <optional>
#include <iostream>

namespace mx::gui {

struct QuickReferenceExplorer::PrivateData final {
  IGeneratorModel *model{nullptr};
  bool closed{false};

  QPushButton *close_button{nullptr};
  QPushButton *save_to_new_ref_explorer_button{nullptr};
  QSizeGrip *size_grip{nullptr};

  std::optional<QPoint> opt_previous_drag_pos;
  QLabel *window_title{nullptr};

  PreviewableReferenceExplorer *reference_explorer{nullptr};
};

QuickReferenceExplorer::QuickReferenceExplorer(
    const Index &index, const FileLocationCache &file_location_cache,
    std::shared_ptr<ITreeGenerator> generator, const bool &show_code_preview,
    IGlobalHighlighter &highlighter, IMacroExplorer &macro_explorer,
    QWidget *parent)
    : QWidget(parent),
      d(new PrivateData) {

  InitializeWidgets(index, file_location_cache, std::move(generator),
                    show_code_preview, highlighter, macro_explorer);

  connect(&IThemeManager::Get(), &IThemeManager::ThemeChanged, this,
          &QuickReferenceExplorer::OnThemeChange);
}

QuickReferenceExplorer::~QuickReferenceExplorer() {}

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

void QuickReferenceExplorer::resizeEvent(QResizeEvent *event) {
  QPoint size_grip_pos(width() - d->size_grip->width(),
                       height() - d->size_grip->height());

  d->size_grip->move(size_grip_pos);

  QWidget::resizeEvent(event);
}

void QuickReferenceExplorer::InitializeWidgets(
    const Index &index, const FileLocationCache &file_location_cache,
    std::shared_ptr<ITreeGenerator> generator, const bool &show_code_preview,
    IGlobalHighlighter &highlighter, IMacroExplorer &macro_explorer) {

  setWindowFlags(Qt::Window | Qt::FramelessWindowHint |
                 Qt::WindowStaysOnTopHint);

  setContentsMargins(5, 5, 5, 5);

  connect(qApp, &QGuiApplication::applicationStateChanged, this,
          &QuickReferenceExplorer::OnApplicationStateChange);

  //
  // Title bar
  //

  d->window_title = new QLabel(tr("Quick reference explorer"));

  // Save as new button
  d->save_to_new_ref_explorer_button = new QPushButton(QIcon(), "", this);
  d->save_to_new_ref_explorer_button->setToolTip(tr("Save to new tab"));
  d->save_to_new_ref_explorer_button->setSizePolicy(QSizePolicy::Minimum,
                                                    QSizePolicy::Minimum);

  connect(d->save_to_new_ref_explorer_button, &QPushButton::clicked, this,
          &QuickReferenceExplorer::OnSaveReferenceExplorer);

  // Close button
  d->close_button = new QPushButton(
      GetIcon(":/Icons/QuickReferenceExplorer/Close"), "", this);

  d->close_button->setToolTip(tr("Close"));
  d->close_button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

  connect(d->close_button, &QPushButton::clicked, this,
          &QuickReferenceExplorer::close);

  // Setup the layout
  auto title_frame_layout = new QHBoxLayout();
  title_frame_layout->setContentsMargins(0, 0, 0, 0);
  title_frame_layout->addWidget(d->window_title);
  title_frame_layout->addStretch();
  title_frame_layout->addWidget(d->save_to_new_ref_explorer_button);
  title_frame_layout->addWidget(d->close_button);

  auto title_frame = new QWidget(this);
  title_frame->installEventFilter(this);
  title_frame->setContentsMargins(0, 0, 0, 0);
  title_frame->setLayout(title_frame_layout);

  UpdateIcons();

  //
  // Contents
  //

  d->model = IGeneratorModel::Create(this);

  connect(d->model, &IGeneratorModel::TreeNameChanged, this,
          &QuickReferenceExplorer::OnTreeNameChanged);

  d->model->InstallGenerator(std::move(generator));

  d->reference_explorer = new PreviewableReferenceExplorer(
      index, file_location_cache, d->model, show_code_preview, highlighter,
      macro_explorer, this);

  connect(d->reference_explorer,
          &PreviewableReferenceExplorer::SelectedItemChanged, this,
          &QuickReferenceExplorer::SelectedItemChanged);

  connect(d->reference_explorer, &PreviewableReferenceExplorer::ItemActivated,
          this, &QuickReferenceExplorer::ItemActivated);

  connect(d->reference_explorer, &PreviewableReferenceExplorer::ExtractSubtree,
          this, &QuickReferenceExplorer::ExtractSubtree);

  d->reference_explorer->setSizePolicy(QSizePolicy::Expanding,
                                       QSizePolicy::Expanding);

  auto contents_layout = new QVBoxLayout();
  contents_layout->setContentsMargins(2, 2, 2, 2);
  contents_layout->addWidget(d->reference_explorer);

  //
  // Main layout
  //

  auto main_layout = new QVBoxLayout();
  main_layout->setContentsMargins(0, 0, 0, 0);
  main_layout->addWidget(title_frame);
  main_layout->addLayout(contents_layout);

  d->size_grip = new QSizeGrip(this);
  d->size_grip->resize(12, 12);

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

  disconnect(d->reference_explorer, nullptr, this, nullptr);

  close();
}

void QuickReferenceExplorer::UpdateIcons() {
  d->save_to_new_ref_explorer_button->setIcon(
      GetIcon(":/Icons/QuickReferenceExplorer/SaveToNewTab"));

  d->close_button->setIcon(GetIcon(":/Icons/QuickReferenceExplorer/Close"));
}

void QuickReferenceExplorer::OnThemeChange(const QPalette &,
                                           const CodeViewTheme &) {
  UpdateIcons();
}

void QuickReferenceExplorer::SetBrowserMode(const bool &enabled) {
  d->reference_explorer->SetBrowserMode(enabled);
}

void QuickReferenceExplorer::OnTreeNameChanged() {
  QString tree_name;
  if (auto tree_name_var =
          d->model->data(QModelIndex(), IGeneratorModel::TreeNameRole);
      tree_name_var.canConvert<QString>()) {

    tree_name = tree_name_var.toString();
  }

  if (tree_name.isEmpty()) {
    tree_name = tr("Unnamed Tree");
  }

  d->window_title->setText(tree_name);
}

}  // namespace mx::gui
