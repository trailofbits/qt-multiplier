/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/ILineEdit.h>

namespace mx::gui {

//! The main implementation for the ILineEdit interface
class LineEdit final : public ILineEdit {
  Q_OBJECT

 public:
  //! Destructor
  virtual ~LineEdit() override;

  //! \copybrief ILineEdit::GetHistory
  virtual QStringList GetHistory() const override;

  //! \copybrief ILineEdit::SetHistory
  virtual void SetHistory(const QStringList &history) override;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Constructor
  LineEdit(QWidget *parent);

 private slots:
  //! This event is received when focus is lost/return is pressed
  void OnEditingFinished();

  friend class ILineEdit;
};

}  // namespace mx::gui
