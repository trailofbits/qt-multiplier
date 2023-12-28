// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QCloseEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPalette>
#include <QResizeEvent>
#include <QShowEvent>
#include <QWidget>

#include <memory>

namespace mx::gui {

class CodeViewTheme;

//! A wrapper class that turns a widget into a popup
class PopupWidget Q_DECL_FINAL : public QWidget {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:

  //! Destructor
  virtual ~PopupWidget(void);

  //! Constructor
  PopupWidget(QWidget *parent = nullptr);

  //! Initializes the internal widgets
  void SetWrappedWidget(QWidget *wrapped_widget);

  //! Returns the wrapped widget
  QWidget *WrappedWidget(void) const;

  //! Disabled copy constructor
  PopupWidget(const PopupWidget &) = delete;
  PopupWidget(PopupWidget &&) noexcept = delete;
  PopupWidget &operator=(const PopupWidget &) = delete;
  PopupWidget &operator=(PopupWidget &&) noexcept = delete;

 protected:
  //! Closes the widget when the escape key is pressed
  void keyPressEvent(QKeyEvent *event) Q_DECL_FINAL;

  //! Helps determine if the widget should be restored on focus
  void showEvent(QShowEvent *event) Q_DECL_FINAL;

  //! Helps determine if the widget should be restored on focus
  void closeEvent(QCloseEvent *event) Q_DECL_FINAL;

  //! Used to update the size grip position
  void resizeEvent(QResizeEvent *event) Q_DECL_FINAL;

  //! Used to handle window movements
  bool eventFilter(QObject *obj, QEvent *event) Q_DECL_FINAL;

 private:

  //! Used to start window dragging
  void OnTitleFrameMousePress(QMouseEvent *event);

  //! Used to move the window by moving the title frame
  void OnTitleFrameMouseMove(QMouseEvent *event);

  //! Used to stop window dragging
  void OnTitleFrameMouseRelease(QMouseEvent *event);

  //! Updates the widget icons to match the active theme
  void UpdateIcons(void);

 private slots:
  //! Restores the widget visibility when the application gains focus
  void OnApplicationStateChange(Qt::ApplicationState state);

  //! Updates the window title at regular intervals
  void OnUpdateTitle(void);

  //! Called by the theme manager
  void OnThemeChange(const QPalette &palette,
                     const CodeViewTheme &code_view_theme);
};

}  // namespace mx::gui
