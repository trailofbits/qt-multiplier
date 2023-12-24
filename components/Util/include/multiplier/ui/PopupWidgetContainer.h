// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/ui/Icons.h>
#include <multiplier/ui/Util.h>
#include <multiplier/ui/IThemeManager.h>

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

//! A wrapper class that turns a widget into a popup
template <typename Widget>
class PopupWidgetContainer final : public QWidget {
 public:
  //! Constructor
  template <typename... Args>
  PopupWidgetContainer(Args &&...args) : QWidget(nullptr),
                                         d(new PrivateData) {

    auto wrapped_widget = new Widget(std::forward<Args>(args)...);
    initializeWidgets(wrapped_widget);

    UpdateIcons();
    connect(&IThemeManager::Get(), &IThemeManager::ThemeChanged, this,
            &PopupWidgetContainer<Widget>::OnThemeChange);

    OnUpdateTitle();
    connect(&d->title_update_timer, &QTimer::timeout, this,
            &PopupWidgetContainer<Widget>::OnUpdateTitle);

    d->title_update_timer.start(500);
  }

  //! Destructor
  virtual ~PopupWidgetContainer() = default;

  //! Returns the wrapped widget
  Widget *GetWrappedWidget() {
    return d->wrapped_widget;
  }

  //! Disabled copy constructor
  PopupWidgetContainer(const PopupWidgetContainer &) = delete;

  //! Disabled copy assignment operator
  PopupWidgetContainer &operator=(const PopupWidgetContainer &) = delete;

 protected:
  //! Closes the widget when the escape key is pressed
  virtual void keyPressEvent(QKeyEvent *event) override {
    if (event->key() == Qt::Key_Escape) {
      close();

    } else {
      QWidget::keyPressEvent(event);
    }
  }

  //! Helps determine if the widget should be restored on focus
  virtual void showEvent(QShowEvent *event) override {
    d->closed = false;

    QWidget::showEvent(event);
  }

  //! Helps determine if the widget should be restored on focus
  virtual void closeEvent(QCloseEvent *event) override {
    d->closed = true;

    QWidget::closeEvent(event);
  }

  //! Used to handle window movements
  virtual bool eventFilter(QObject *obj, QEvent *event) override {
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

  //! Used to update the size grip position
  virtual void resizeEvent(QResizeEvent *event) override {
    QPoint size_grip_pos(width() - d->size_grip->width(),
                         height() - d->size_grip->height());

    d->size_grip->move(size_grip_pos);
    QWidget::resizeEvent(event);
  }

 private:
  //! Private class data
  struct PrivateData final {
    bool closed{false};

    QPushButton *close_button{nullptr};
    QLabel *window_title{nullptr};
    Widget *wrapped_widget{nullptr};

    std::optional<QPoint> opt_previous_drag_pos;
    QSizeGrip *size_grip{nullptr};

    QTimer title_update_timer;
  };

  std::unique_ptr<PrivateData> d;

  //! Initializes the internal widgets
  void initializeWidgets(Widget *wrapped_widget) {
    d->wrapped_widget = wrapped_widget;
    setAttribute(Qt::WA_QuitOnClose, false);

    setContentsMargins(5, 5, 5, 5);
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint |
                   Qt::WindowStaysOnTopHint);

    QWidget::connect(qApp, &QGuiApplication::applicationStateChanged, this,
                     &PopupWidgetContainer<Widget>::OnApplicationStateChange);

    d->window_title = new QLabel(d->wrapped_widget->windowTitle());

    d->close_button = new QPushButton(QIcon(), "", this);
    d->close_button->setToolTip(QObject::tr("Close"));
    d->close_button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    QWidget::connect(d->close_button, &QPushButton::clicked, this,
                     &PopupWidgetContainer<Widget>::close);

    auto title_frame_layout = new QHBoxLayout();
    title_frame_layout->setContentsMargins(0, 0, 0, 0);
    title_frame_layout->addWidget(d->window_title);
    title_frame_layout->addStretch();
    title_frame_layout->addWidget(d->close_button);

    auto title_frame = new QWidget(this);
    title_frame->installEventFilter(this);
    title_frame->setContentsMargins(0, 0, 0, 0);
    title_frame->setLayout(title_frame_layout);

    auto contents_layout = new QVBoxLayout();
    contents_layout->setContentsMargins(0, 0, 0, 0);
    contents_layout->addWidget(d->wrapped_widget);
    contents_layout->addStretch();

    auto main_layout = new QVBoxLayout();
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->addWidget(title_frame);
    main_layout->addLayout(contents_layout);
    main_layout->addStretch();

    setLayout(main_layout);

    d->size_grip = new QSizeGrip(this);
    d->size_grip->resize(12, 12);
  }

  //! Used to start window dragging
  void OnTitleFrameMousePress(QMouseEvent *event) {
    d->opt_previous_drag_pos = event->globalPosition().toPoint();
  }

  //! Used to move the window by moving the title frame
  void OnTitleFrameMouseMove(QMouseEvent *event) {
    if (!d->opt_previous_drag_pos.has_value()) {
      return;
    }

    auto &previous_drag_pos = d->opt_previous_drag_pos.value();

    auto diff = event->globalPosition().toPoint() - previous_drag_pos;
    previous_drag_pos = event->globalPosition().toPoint();

    move(x() + diff.x(), y() + diff.y());
  }

  //! Used to stop window dragging
  void OnTitleFrameMouseRelease(QMouseEvent *event) {
    d->opt_previous_drag_pos = std::nullopt;
  }

  //! Updates the widget icons to match the active theme
  void UpdateIcons() {
    d->close_button->setIcon(GetIcon(":/Icons/PopupWidgetContainer/close"));
  }

 private slots:
  //! Restores the widget visibility when the application gains focus
  void OnApplicationStateChange(Qt::ApplicationState state) {
    if (d->closed) {
      return;
    }

    auto window_is_visible = state == Qt::ApplicationActive;
    setVisible(window_is_visible);
  }

  //! Updates the window title at regular intervals
  void OnUpdateTitle() {
    setWindowTitle(d->wrapped_widget->windowTitle());
    d->window_title->setText(windowTitle());
  }

  //! Called by the theme manager
  void OnThemeChange(const QPalette &palette,
                     const CodeViewTheme &code_view_theme) {
    UpdateIcons();
  }
};

}  // namespace mx::gui
