// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Widgets/SimpleTextInputDialog.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>

namespace mx::gui {

struct SimpleTextInputDialog::PrivateData final {
  std::optional<QString> opt_text_input;
};

SimpleTextInputDialog::SimpleTextInputDialog(
    const QString &question, const std::optional<QString> &opt_default_text,
    QWidget *parent)
    : QDialog(parent),
      d(new PrivateData) {
  setWindowTitle(tr("Question"));
  InitializeWidgets(question, opt_default_text);
}

SimpleTextInputDialog::~SimpleTextInputDialog(void) {}

std::optional<QString> SimpleTextInputDialog::TextInput(void) const {
  return d->opt_text_input;
}

void SimpleTextInputDialog::InitializeWidgets(
    const QString &question, const std::optional<QString> &opt_default_text) {

  auto main_layout = new QVBoxLayout();
  main_layout->addWidget(new QLabel(question, this));

  auto text_input = new QLineEdit(this);
  if (opt_default_text.has_value()) {
    text_input->setText(opt_default_text.value());
  }

  main_layout->addWidget(text_input);

  auto buttons_layout = new QHBoxLayout();
  buttons_layout->addStretch();

  auto reject_button = new QPushButton(tr("Cancel"), this);
  buttons_layout->addWidget(reject_button);

  auto accept_button = new QPushButton(tr("Ok"), this);
  accept_button->setDefault(true);
  accept_button->setAutoDefault(true);
  buttons_layout->addWidget(accept_button);

  main_layout->addStretch();
  main_layout->addLayout(buttons_layout);

  setLayout(main_layout);

  connect(reject_button, &QPushButton::pressed,
          this, &QDialog::reject);

  connect(accept_button, &QPushButton::pressed,
          this, &QDialog::accept);

  connect(text_input, &QLineEdit::textEdited,
          this, &SimpleTextInputDialog::OnTextEdited);

  d->opt_text_input = opt_default_text;
}

void SimpleTextInputDialog::OnTextEdited(const QString &text) {
  if (text.isEmpty()) {
    d->opt_text_input = std::nullopt;
  } else {
    d->opt_text_input = text;
  }
}

}  // namespace mx::gui
