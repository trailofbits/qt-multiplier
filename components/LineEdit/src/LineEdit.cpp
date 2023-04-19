/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "LineEdit.h"

#include <QCompleter>
#include <QStringListModel>

namespace mx::gui {

namespace {

const int kMaxVisibleHistoryItems{10};

}

struct LineEdit::PrivateData final {
  QCompleter *completer{nullptr};
  QStringListModel history_model;
};

LineEdit::~LineEdit() {}

QStringList LineEdit::GetHistory() const {
  return d->history_model.stringList();
}

void LineEdit::SetHistory(const QStringList &history) {
  d->history_model.setStringList(history);
}

LineEdit::LineEdit(QWidget *parent) : ILineEdit(parent), d(new PrivateData()) {
  d->completer = new QCompleter(this);
  d->completer->setModel(&d->history_model);
  d->completer->setModelSorting(QCompleter::CaseInsensitivelySortedModel);
  d->completer->setMaxVisibleItems(kMaxVisibleHistoryItems);

  setCompleter(d->completer);

  connect(this, &QLineEdit::editingFinished, this,
          &LineEdit::OnEditingFinished);
}

void LineEdit::OnEditingFinished() {
  auto history_item = text();
  if (history_item.isEmpty()) {
    return;
  }

  auto history = GetHistory();
  history.append(history_item);

  SetHistory(history);
}

}  // namespace mx::gui
