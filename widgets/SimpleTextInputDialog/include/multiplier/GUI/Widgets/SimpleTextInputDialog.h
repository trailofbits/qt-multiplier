// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QDialog>
#include <QString>

#include <optional>
#include <memory>

namespace mx::gui {

//! A simple dialog that requests text input from the user
class SimpleTextInputDialog final : public QDialog {
  Q_OBJECT

  struct PrivateData;
  const std::unique_ptr<PrivateData> d;

 public:
  //! Constructor
  SimpleTextInputDialog(const QString &question,
                        const std::optional<QString> &opt_default_text,
                        QWidget *parent = nullptr);

  //! Destructor
  virtual ~SimpleTextInputDialog(void) override;

  //! Returns the text input, if any
  std::optional<QString> TextInput(void) const;

 private:
  //! Initializes the internal widgets
  void InitializeWidgets(const QString &question,
                         const std::optional<QString> &opt_default_text);

 private slots:
  //! Called when the input text changes
  void OnTextEdited(const QString &text);
};

}  // namespace mx::gui
