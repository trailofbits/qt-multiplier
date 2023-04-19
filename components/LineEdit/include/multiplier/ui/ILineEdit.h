/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QLineEdit>

namespace mx::gui {

//! A line edit that remembers history
class ILineEdit : public QLineEdit {
  Q_OBJECT

 public:
  //! Factory method
  static ILineEdit *Create(QWidget *parent = nullptr);

  //! Constructor
  ILineEdit(QWidget *parent) : QLineEdit(parent) {}

  //! Destructor
  virtual ~ILineEdit() override = default;

  //! Returns the current history
  virtual QStringList GetHistory() const = 0;

  //! Sets the history
  virtual void SetHistory(const QStringList &history) = 0;

  //! Disabled copy constructor
  ILineEdit(const ILineEdit &) = delete;

  //! Disabled copy assignment operator
  ILineEdit &operator=(const ILineEdit &) = delete;
};

}  // namespace mx::gui
