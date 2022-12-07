/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "CodeView2.h"

#include <QDebug>
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

namespace mx {

namespace {

class QPlainTextEditMod final : public QPlainTextEdit {
 public:
  QPlainTextEditMod(QWidget *parent = nullptr) : QPlainTextEdit(parent) {}
  virtual ~QPlainTextEditMod() override{};

  friend class mx::CodeView2;
};

struct ModelCursor final {
  std::size_t active_line_size{};
  QModelIndex parent;
  int row{};
  int column{};
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

  connect(d->model, &ICodeModel::dataChanged, this, &CodeView2::OnDataChanged);
}

void CodeView2::InitializeWidgets() {
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

  OnDataChanged();
}

void CodeView2::OnDataChanged() {
  d->text_edit->clear();
  return;

  auto document = new QTextDocument(this);
  auto document_layout = new QPlainTextDocumentLayout(document);
  document->setDocumentLayout(document_layout);
  d->text_edit->setDocument(document);

  std::stack<ModelCursor> model_cursor_stack;
  model_cursor_stack.push({0, QModelIndex(), 0, 0});

  auto cursor = std::make_unique<QTextCursor>(document);

  for (;;) {
  restart:
    if (model_cursor_stack.empty()) {
      break;
    }

    auto model_cursor = model_cursor_stack.top();
    model_cursor_stack.pop();

    model_cursor.active_line_size = 0;

    auto row_count = d->model->rowCount(model_cursor.parent);

    for (; model_cursor.row < row_count; ++model_cursor.row) {
      for (;; ++model_cursor.column) {
        auto index = d->model->index(model_cursor.row, model_cursor.column,
                                     model_cursor.parent);

        auto display_role = d->model->data(index, Qt::DisplayRole);
        if (!display_role.isValid()) {
          break;
        }

        const auto &buffer = display_role.toString();
        model_cursor.active_line_size +=
            static_cast<std::size_t>(buffer.size());

        cursor->insertText(buffer);

        if (d->model->rowCount(index) > 0) {
          model_cursor_stack.push(model_cursor);
          model_cursor_stack.push({0, index, 0, 0});

          cursor->insertText("\n");

          goto restart;
        }
      }

      model_cursor.column = 0;
      cursor->insertText("\n");
    }
  }

  d->gutter->setMinimumWidth(100);
}

void CodeView2::OnCursorPositionChange() {
  // TODO(alessandro): This should be either computed or part of the theme
  /*QTextEdit::ExtraSelection selection;
  selection.format.setBackground(QColor(Qt::black));
  selection.format.setProperty(QTextFormat::FullWidthSelection, true);
  selection.cursor = d->text_edit->textCursor();
  selection.cursor.clearSelection();

  d->text_edit->setExtraSelections({std::move(selection)});*/
}

void CodeView2::OnGutterPaintEvent(QPaintEvent *event) {
  QPainter painter(d->gutter);

  const auto &base_color = d->text_edit->palette().base();
  painter.fillRect(event->rect(), base_color);
}

void CodeView2::OnTextEditPaintEvent(QPaintEvent *event) {
  QPainter painter(d->gutter);

  const auto &base_color = d->text_edit->palette().base();
  painter.fillRect(event->rect(), base_color);
}

void CodeView2::OnTextEditUpdateRequest(const QRect &, int) {
  d->gutter->update();
}

}  // namespace mx
