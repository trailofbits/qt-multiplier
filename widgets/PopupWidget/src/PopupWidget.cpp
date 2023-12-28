// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Widgets/PopupWidget.h>

#include <multiplier/GUI/Icons.h>
#include <multiplier/GUI/Util.h>
#include <multiplier/GUI/ThemeManager.h>

#include <optional>

#include <QWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QSizeGrip>
#include <QLabel>
#include <QApplication>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QMouseEvent>
#include <QTimer>

namespace mx::gui {

//! Private class data
struct PopupWidget::PrivateData final {
  bool closed{false};

  QPushButton *close_button{nullptr};
  QLabel *window_title{nullptr};
  QWidget *wrapped_widget{nullptr};
  QVBoxLayout *main_layout{nullptr};

  std::optional<QPoint> opt_previous_drag_pos;
  QSizeGrip *size_grip{nullptr};

  QTimer title_update_timer;
};

PopupWidget::~PopupWidget(void) {}

//! Constructor
PopupWidget::PopupWidget(QWidget *parent)
    : QWidget(parent),
      d(new PrivateData) {}

//! Initializes the internal widgets
void PopupWidget::SetWrappedWidget(QWidget *wrapped_widget) {
  d->wrapped_widget = wrapped_widget;
  setAttribute(Qt::WA_QuitOnClose, false);

  setContentsMargins(5, 5, 5, 5);
  setWindowFlags(Qt::Window | Qt::FramelessWindowHint |
                 Qt::WindowStaysOnTopHint);

  QWidget::connect(qApp, &QGuiApplication::applicationStateChanged, this,
                   &PopupWidget<Widget>::OnApplicationStateChange);

  QString inherited_title = d->wrapped_widget->windowTitle();
  setWindowTitle(inherited_title);
  if (!d->window_title) {
    d->window_title = new QLabel(inherited_title);

    connect(&d->title_update_timer, &QTimer::timeout,
            this, &PopupWidget::OnUpdateTitle);
  
  } else {
    d->window_title->setText(inherited_title);
  }

  if (!d->close_button) {
    d->close_button = new QPushButton(QIcon(), "", this);
    d->close_button->setToolTip(QObject::tr("Close"));
    d->close_button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    QWidget::connect(d->close_button, &QPushButton::clicked, this,
                     &PopupWidget::close);

  auto created = !d->main_layout;
  if (!d->main_layout) {
    auto title_frame_layout = new QHBoxLayout();
    title_frame_layout->setContentsMargins(0, 0, 0, 0);
    title_frame_layout->addWidget(d->window_title);
    title_frame_layout->addStretch();
    title_frame_layout->addWidget(d->close_button);

    auto title_frame = new QWidget(this);
    title_frame->installEventFilter(this);
    title_frame->setContentsMargins(0, 0, 0, 0);
    title_frame->setLayout(title_frame_layout);

    d->main_layout = new QVBoxLayout();
    d->main_layout->setContentsMargins(0, 0, 0, 0);
    d->main_layout->addWidget(title_frame);

  } else {
    auto prev_widget = d->main_layout->takeAt(1);
    prev_widget->setParent(nullptr);
    prev_widget->deleteLater();
  }

  d->wrapped_widget->setSizePolicy(QSizePolicy::Expanding,
                                   QSizePolicy::Expanding);

  d->main_layout->addWidget(d->wrapped_widget);
  d->main_layout->addStretch();

  if (created) {
    setLayout(main_layout);
  }

  if (!d->size_grip) {
    d->size_grip = new QSizeGrip(this);
    d->size_grip->resize(12, 12);

    UpdateIcons();
    connect(&ThemeManager::Get(), &ThemeManager::ThemeChanged, this,
            &PopupWidget::OnThemeChange);
  }

  OnUpdateTitle();
  d->title_update_timer.start(500);
}

//! Returns the wrapped widget
QWidget *PopupWidget::WrappedWidget(void) const {
  return d->wrapped_widget;
}

//! Closes the widget when the escape key is pressed
void PopupWidget::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    close();

  } else {
    QWidget::keyPressEvent(event);
  }
}

//! Helps determine if the widget should be restored on focus
void PopupWidget::showEvent(QShowEvent *event) {
  d->closed = false;

  QWidget::showEvent(event);
}

//! Helps determine if the widget should be restored on focus
void PopupWidget::closeEvent(QCloseEvent *event) {
  d->closed = true;

  QWidget::closeEvent(event);
}

//! Used to update the size grip position
void PopupWidget::resizeEvent(QResizeEvent *event) {
  QPoint size_grip_pos(width() - d->size_grip->width(),
                       height() - d->size_grip->height());
  d->size_grip->move(size_grip_pos);
  QWidget::resizeEvent(event);
}

//! Used to handle window movements
bool PopupWidget::eventFilter(QObject *obj, QEvent *event) {
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

  return QWidget::eventFilter(obj, event);
}

//! Used to start window dragging
void PopupWidget::OnTitleFrameMousePress(QMouseEvent *event) {
  d->opt_previous_drag_pos = event->globalPosition().toPoint();
}

//! Used to move the window by moving the title frame
void PopupWidget::OnTitleFrameMouseMove(QMouseEvent *event) {
  if (!d->opt_previous_drag_pos.has_value()) {
    return;
  }

  auto &previous_drag_pos = d->opt_previous_drag_pos.value();

  auto diff = event->globalPosition().toPoint() - previous_drag_pos;
  previous_drag_pos = event->globalPosition().toPoint();

  move(x() + diff.x(), y() + diff.y());
}

//! Used to stop window dragging
void PopupWidget::OnTitleFrameMouseRelease(QMouseEvent *event) {
  d->opt_previous_drag_pos = std::nullopt;
}

//! Updates the widget icons to match the active theme
void PopupWidget::UpdateIcons(void) {
  d->close_button->setIcon(GetIcon(":/Icons/PopupWidget/close"));
}

//! Restores the widget visibility when the application gains focus
void PopupWidget::OnApplicationStateChange(Qt::ApplicationState state) {
  if (d->closed) {
    return;
  }

  auto window_is_visible = state == Qt::ApplicationActive;
  setVisible(window_is_visible);
}

//! Updates the window title at regular intervals
void PopupWidget::OnUpdateTitle(void) {
  setWindowTitle(d->wrapped_widget->windowTitle());
  d->window_title->setText(windowTitle());
}

//! Called by the theme manager
void PopupWidget::OnThemeChange(const QPalette &palette,
                   const CodeViewTheme &code_view_theme) {
  UpdateIcons();
}

}  // namespace mx::gui
