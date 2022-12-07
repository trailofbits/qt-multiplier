/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "CodeView2.h"

#include <QFont>
#include <QHBoxLayout>
#include <QPaintEvent>
#include <QPainter>
#include <QPlainTextDocumentLayout>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QWidget>
#include <memory>
#include <stack>
#include <unordered_map>
#include <unordered_set>

namespace mx::gui {

namespace {

class QPlainTextEditMod final : public QPlainTextEdit {
 public:
  QPlainTextEditMod(QWidget *parent = nullptr) : QPlainTextEdit(parent) {}
  virtual ~QPlainTextEditMod() override{};

  friend class mx::gui::CodeView2;
};

}  // namespace

struct CodeView2::PrivateData final {
  ICodeModel *model{nullptr};
  QPlainTextEditMod *text_edit{nullptr};

  QWidget *gutter{nullptr};
};

CodeView2::~CodeView2() {}

CodeView2::CodeView2(ICodeModel *model, QWidget *parent)
    : ICodeView2(parent),
      d(new PrivateData) {

  InstallModel(model);
  InitializeWidgets();
}

bool CodeView2::eventFilter(QObject *obj, QEvent *event) {
  if (event->type() == QEvent::Paint) {
    auto paint_event = static_cast<QPaintEvent *>(event);

    if (obj == d->gutter) {
      OnGutterPaintEvent(paint_event);

    } else if (obj == d->text_edit) {
      OnTextEditPaintEvent(paint_event);
    }

    return false;
  }

  return true;
}

void CodeView2::InstallModel(ICodeModel *model) {
  d->model = model;
  d->model->setParent(this);

  connect(d->model, &ICodeModel::ModelReset, this, &CodeView2::OnModelReset);
}

void CodeView2::InitializeWidgets() {
  // TODO(alessandro): This should be part of the theme
  QFont font("Monaco");
  font.setStyleHint(QFont::TypeWriter);
  setFont(font);

  d->text_edit = new QPlainTextEditMod();
  d->text_edit->setFont(font);
  d->text_edit->setReadOnly(true);
  d->text_edit->setOverwriteMode(false);
  d->text_edit->setTextInteractionFlags(Qt::TextSelectableByMouse);
  d->text_edit->installEventFilter(this);

  d->gutter = new QWidget();
  d->gutter->setFont(font);
  d->gutter->installEventFilter(this);

  auto layout = new QHBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(d->gutter);
  layout->addWidget(d->text_edit);
  setLayout(layout);

  connect(d->text_edit, &QPlainTextEditMod::cursorPositionChanged, this,
          &CodeView2::OnCursorPositionChange);

  connect(d->text_edit, &QPlainTextEditMod::updateRequest, this,
          &CodeView2::OnTextEditUpdateRequest);

  OnModelReset();
}

void CodeView2::OnModelReset() {
  d->text_edit->clear();

  auto document = new QTextDocument(this);
  auto document_layout = new QPlainTextDocumentLayout(document);
  document->setDocumentLayout(document_layout);
  d->text_edit->setDocument(document);

  auto cursor = std::make_unique<QTextCursor>(document);

  auto row_count = d->model->RowCount();
  for (int row_index = 0; row_index < row_count; ++row_index) {
    auto token_count = d->model->TokenCount(row_index);
    for (int token_index = 0; token_index < token_count; ++token_index) {
      const auto &token_var =
          d->model->Data({row_index, token_index}, Qt::DisplayRole);

      if (!token_var.isValid()) {
        continue;
      }

      const auto &token = token_var.toString();
      cursor->insertText(token);
    }

    cursor->insertText("\n");
  }

  d->gutter->setMinimumWidth(100);
}

void CodeView2::OnCursorPositionChange() {
  // TODO(alessandro): This should be part of the theme
  QTextEdit::ExtraSelection selection;
  selection.format.setBackground(QColor(Qt::black));
  selection.format.setProperty(QTextFormat::FullWidthSelection, true);
  selection.cursor = d->text_edit->textCursor();
  selection.cursor.clearSelection();

  d->text_edit->setExtraSelections({std::move(selection)});
}

void CodeView2::OnGutterPaintEvent(QPaintEvent *event) {
  QPainter painter(d->gutter);

  const auto &base_color = d->text_edit->palette().base();
  painter.fillRect(event->rect(), base_color);
}

void CodeView2::OnTextEditPaintEvent(QPaintEvent *event) {
  QPainter painter(d->text_edit);

  const auto &base_color = d->text_edit->palette().base();
  painter.fillRect(event->rect(), base_color);
}

void CodeView2::OnTextEditUpdateRequest(const QRect &, int) {
  d->gutter->update();
}

}  // namespace mx::gui
