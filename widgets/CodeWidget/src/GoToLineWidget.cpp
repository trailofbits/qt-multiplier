/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "GoToLineWidget.h"

#include <QVBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QEvent>
#include <QResizeEvent>
#include <QShortcut>
#include <QIntValidator>

namespace mx::gui {

struct GoToLineWidget::PrivateData final {
  QLineEdit *line_number_edit{nullptr};
  QIntValidator *line_number_validator{nullptr};
  unsigned max_line_number{};
  QShortcut *deactivate_shortcut{nullptr};
};

GoToLineWidget::GoToLineWidget(QWidget *parent)
    : QWidget(parent),
      d(new PrivateData) {
  InitializeWidgets(parent);
}

GoToLineWidget::~GoToLineWidget() {}

void GoToLineWidget::InitializeWidgets(QWidget *parent) {
  d->line_number_edit = new QLineEdit(this);
  connect(d->line_number_edit, &QLineEdit::editingFinished, this,
          &GoToLineWidget::OnLineNumberInputChanged);

  d->line_number_validator = new QIntValidator(0, 0, this);
  d->line_number_edit->setValidator(d->line_number_validator);

  auto layout = new QVBoxLayout();
  layout->addWidget(d->line_number_edit);
  setLayout(layout);

  parent->installEventFilter(this);
  UpdateWidgetPlacement();

  d->deactivate_shortcut = new QShortcut(QKeySequence::Cancel, this, this,
                                         &GoToLineWidget::Deactivate,
                                         Qt::WidgetWithChildrenShortcut);

  setVisible(false);
}

bool GoToLineWidget::eventFilter(QObject *obj, QEvent *event) {
  if (event->type() == QEvent::Resize && obj == parent()) {
    UpdateWidgetPlacement();
  }

  return QWidget::eventFilter(obj, event);
}

void GoToLineWidget::UpdateWidgetPlacement(void) {
  auto &parent_widget = *static_cast<QWidget *>(parent());
  auto parent_size = parent_widget.size();

  auto widget_width = parent_size.width() / 2;
  auto widget_height = sizeHint().height();

  auto widget_x = (parent_size.width() / 2) - (widget_width / 2);

  resize(widget_width, widget_height);
  move(widget_x, 0);

  raise();
}

void GoToLineWidget::OnLineNumberInputChanged(void) {
  auto line_number = d->line_number_edit->text().toUInt();
  emit LineNumberChanged(line_number);

  Deactivate();
}

void GoToLineWidget::Activate(unsigned max_line_number) {
  d->max_line_number = max_line_number;

  d->line_number_edit->clear();
  d->line_number_edit->setPlaceholderText(tr("Enter a line number from 1 to ") +
                                          QString::number(max_line_number));

  d->line_number_validator->setRange(1, static_cast<int>(max_line_number));

  setVisible(true);
  d->line_number_edit->setFocus();
}

void GoToLineWidget::Deactivate() {
  setVisible(false);
}

}  // namespace mx::gui