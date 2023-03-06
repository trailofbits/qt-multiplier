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

 public:
  //! Constructor
  SimpleTextInputDialog(const QString &question,
                        const std::optional<QString> &opt_default_text,
                        QWidget *parent = nullptr);

  //! Destructor
  virtual ~SimpleTextInputDialog() override;

  //! Returns the text input, if any
  const std::optional<QString> &GetTextInput() const;

  //! Disabled copy constructor
  SimpleTextInputDialog(const SimpleTextInputDialog &) = delete;

  //! Disabled copy assignment operator
  SimpleTextInputDialog &operator=(const SimpleTextInputDialog &) = delete;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Initializes the internal widgets
  void InitializeWidgets(const QString &question,
                         const std::optional<QString> &opt_default_text);

 private slots:
  //! Called when the input text changes
  void OnTextEdited(const QString &text);
};

}  // namespace mx::gui
