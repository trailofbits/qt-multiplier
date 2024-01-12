// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QWidget>

QT_BEGIN_NAMESPACE
class QCloseEvent;
class QHideEvent;
class QKeySequence;
class QModelIndex;
class QShowEvent;
QT_END_NAMESPACE

namespace mx::gui {

//! A widget that can be managed by the window manager.
class IWindowWidget : public QWidget {
  Q_OBJECT

 public:
  virtual ~IWindowWidget(void);
  using QWidget::QWidget;

 protected:
  void hideEvent(QHideEvent *) Q_DECL_OVERRIDE;
  void showEvent(QShowEvent *) Q_DECL_OVERRIDE;
  void closeEvent(QCloseEvent *) Q_DECL_OVERRIDE;

 signals:
  //! Notify the window manager that this widget has been shown.
  void Shown(void);

  //! Notify the window manager that this widget has been hidden.
  void Hidden(void);

  //! Notify the window manager that this widget has been closed.
  void Closed(void);

  //! Request a primary click action.
  void RequestPrimaryClick(const QModelIndex &index);

  //! Request a secondary click action.
  void RequestSecondaryClick(const QModelIndex &index);

  //! Request a key press action.
  void RequestKeyPress(const QKeySequence &keys, const QModelIndex &index);
};

}  // namespace mx::gui

Q_DECLARE_INTERFACE(mx::gui::IWindowWidget,
                    "com.trailofbits.interface.IWindowWidget")
