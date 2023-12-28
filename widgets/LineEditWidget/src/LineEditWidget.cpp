/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/ui/LineEditWidget.h>

#include <QCompleter>
#include <QStringListModel>

namespace mx::gui {
namespace {

const int kMaxVisibleHistoryItems{10};

}  // namespace

struct LineEditWidget::PrivateData final {
  QCompleter *completer{nullptr};
  QStringListModel history_model;
};

LineEditWidget::~LineEditWidget() {}

QStringList LineEditWidget::History(void) const {
  return d->history_model.stringList();
}

void LineEditWidget::SetHistory(const QStringList &history) {
  d->history_model.setStringList(history);
}

LineEditWidget::LineEditWidget(QWidget *parent)
    : QLineEdit(parent),
      d(new PrivateData) {

  d->completer = new QCompleter(this);
  d->completer->setModel(&d->history_model);
  d->completer->setModelSorting(QCompleter::CaseInsensitivelySortedModel);
  d->completer->setMaxVisibleItems(kMaxVisibleHistoryItems);

  setCompleter(d->completer);

  connect(this, &QLineEdit::editingFinished,
          this, &LineEditWidget::OnEditingFinished);
}

void LineEditWidget::OnEditingFinished(void) {
  auto history_item = text();
  if (history_item.isEmpty()) {
    return;
  }

  auto history = GetHistory();
  if (history.contains(history_item)) {
    return;
  }

  history.append(history_item);
  SetHistory(history);
}

}  // namespace mx::gui
