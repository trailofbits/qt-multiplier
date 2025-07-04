/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QWidget>

#include <memory>

namespace mx::gui {

//! A floating input widget used for the go-to-line shortcut
class GoToLineWidget Q_DECL_FINAL : public QWidget {
  Q_OBJECT

 public:
  //! Constructor
  GoToLineWidget(QWidget *parent);

  //! Destructor
  virtual ~GoToLineWidget(void);

  //! Disabled copy constructor
  GoToLineWidget(const GoToLineWidget &) = delete;

  //! Disabled assignment
  GoToLineWidget &operator=(const GoToLineWidget &) = delete;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Initializes the widget components
  void InitializeWidgets(QWidget *parent);

  //! Used to capture resize events
  bool eventFilter(QObject *obj, QEvent *event) Q_DECL_FINAL;

  //! Updates the widget placement based on the parent size
  void UpdateWidgetPlacement(void);

 private slots:
  //! Called when the line edit is edited by the user
  void OnLineNumberInputChanged(void);
  void Disable(void);

 public slots:
  //! Shows the widget
  void Activate(unsigned max_line_number);

  //! Hides the widget
  void Deactivate(void);

 signals:
  void LineNumberChanged(unsigned line_number);
};

}  // namespace mx::gui