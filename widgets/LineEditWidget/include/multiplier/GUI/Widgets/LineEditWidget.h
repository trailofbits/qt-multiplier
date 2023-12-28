/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QLineEdit>

namespace mx::gui {

//! A line edit that remembers history.
class LineEditWidget Q_DECL_FINAL : public QLineEdit {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:

  //! Constructor
  LineEdit(QWidget *parent);

  //! Destructor
  virtual ~LineEditWidget(void) override;

  //! Returns the current history
  QStringList History(void) const;

  //! Sets the history
  void SetHistory(const QStringList &history) override;

 private:

 private slots:
  //! This event is received when focus is lost/return is pressed
  void OnEditingFinished(void);
};

}  // namespace mx::gui
