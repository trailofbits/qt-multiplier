/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "CodeView.h"

#include <QFont>
#include <QHBoxLayout>
#include <QMetaMethod>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPlainTextDocumentLayout>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QWidget>
#include <QProgressDialog>

#include <iostream>
#include <memory>
#include <optional>
#include <stack>
#include <unordered_map>
#include <unordered_set>

#include "DefaultCodeViewThemes.h"

namespace mx::gui {

namespace {

class QPlainTextEditMod final : public QPlainTextEdit {
 public:
  QPlainTextEditMod(QWidget *parent = nullptr) : QPlainTextEdit(parent) {}
  virtual ~QPlainTextEditMod() override{};

  friend class mx::gui::CodeView;
};

struct TextBlockIndexEntry final {
  int start_position{};
  int end_position{};
  CodeModelIndex index;
};

using TextBlockIndex = std::vector<TextBlockIndexEntry>;

QColor GetTextColorMapEntry(
    const QVariant &token_category_var, const QColor &default_color,
    const std::unordered_map<TokenCategory, QColor> &color_map) {

  if (!token_category_var.isValid()) {
    return default_color;
  }

  const auto &token_category =
      static_cast<TokenCategory>(token_category_var.toUInt());

  auto it = color_map.find(token_category);
  if (it == color_map.end()) {
    return default_color;
  } else {
    return it->second;
  }
}

QColor GetTextBackgroundColor(const CodeViewTheme &code_theme,
                              const QVariant &token_category_var) {
  return GetTextColorMapEntry(token_category_var,
                              code_theme.default_background_color,
                              code_theme.token_background_color_map);
}

QColor GetTextForegroundColor(const CodeViewTheme &code_theme,
                              const QVariant &token_category_var) {
  return GetTextColorMapEntry(token_category_var,
                              code_theme.default_foreground_color,
                              code_theme.token_foreground_color_map);
}

CodeViewTheme::Style GetTextStyle(const CodeViewTheme &code_theme,
                                  const QVariant &token_category_var) {

  if (!token_category_var.isValid()) {
    return {};
  }

  const auto &token_category =
      static_cast<TokenCategory>(token_category_var.toUInt());

  auto it = code_theme.token_style_map.find(token_category);
  if (it == code_theme.token_style_map.end()) {
    return {};
  } else {
    return it->second;
  }
}

}  // namespace

struct CodeView::PrivateData final {
  ICodeModel *model{nullptr};

  QPlainTextEditMod *text_edit{nullptr};
  QWidget *gutter{nullptr};

  TextBlockIndex text_block_index;
  CodeViewTheme theme;

  std::optional<CodeModelIndex> opt_prev_hovered_model_index;
};

void CodeView::setTheme(const CodeViewTheme &theme) {
  d->theme = theme;

  OnModelReset();
}

CodeView::~CodeView() {}

CodeView::CodeView(ICodeModel *model, QWidget *parent)
    : ICodeView(parent),
      d(new PrivateData) {

  InstallModel(model);
  InitializeWidgets();
}

bool CodeView::eventFilter(QObject *obj, QEvent *event) {
  if (event->type() == QEvent::Paint) {
    auto paint_event = static_cast<QPaintEvent *>(event);

    if (obj == d->gutter) {
      OnGutterPaintEvent(paint_event);
      return true;
    }

    return false;

  } else if (event->type() == QEvent::MouseMove) {
    auto mouse_event = static_cast<QMouseEvent *>(event);

    if (obj == d->text_edit->viewport()) {
      OnTextEditViewportMouseMoveEvent(mouse_event);
      return false;
    }

    return false;

  } else if (event->type() == QEvent::MouseButtonRelease) {
    auto mouse_event = static_cast<QMouseEvent *>(event);

    if (obj == d->text_edit->viewport()) {
      OnTextEditViewportMouseButtonReleaseEvent(mouse_event);
      return false;
    }

    return false;

  } else if (event->type() == QEvent::MouseButtonDblClick) {
    auto mouse_event = static_cast<QMouseEvent *>(event);

    if (obj == d->text_edit->viewport()) {
      OnTextEditViewportMouseButtonDblClick(mouse_event);
      return false;
    }

    return false;
  }

  return false;
}

void CodeView::InstallModel(ICodeModel *model) {
  d->model = model;
  d->model->setParent(this);

  connect(d->model, &ICodeModel::ModelReset, this, &CodeView::OnModelReset);
}

void CodeView::InitializeWidgets() {
  d->theme = kDefaultDarkCodeViewTheme;

  // TODO(alessandro): This should be part of the theme
  QFont font("Monaco");
  font.setStyleHint(QFont::TypeWriter);
  setFont(font);

  d->text_edit = new QPlainTextEditMod();
  d->text_edit->setFont(font);
  d->text_edit->setReadOnly(true);
  d->text_edit->setOverwriteMode(false);
  d->text_edit->setTextInteractionFlags(Qt::TextSelectableByMouse);
  d->text_edit->viewport()->installEventFilter(this);
  d->text_edit->viewport()->setMouseTracking(true);

  d->gutter = new QWidget();
  d->gutter->setFont(font);
  d->gutter->installEventFilter(this);

  auto layout = new QHBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  layout->addWidget(d->gutter);
  layout->addWidget(d->text_edit);
  setLayout(layout);

  connect(d->text_edit, &QPlainTextEditMod::cursorPositionChanged, this,
          &CodeView::OnCursorPositionChange);

  connect(d->text_edit, &QPlainTextEditMod::updateRequest, this,
          &CodeView::OnTextEditUpdateRequest);

  OnModelReset();
}

std::optional<CodeModelIndex>
CodeView::ModelIndexFromMousePosition(QPoint pos) {
  auto text_cursor = d->text_edit->cursorForPosition(pos);
  auto cursor_position = text_cursor.position();

  // clang-format off
  auto text_block_index_it = std::find_if(
    d->text_block_index.begin(),
    d->text_block_index.end(),

    [&](const TextBlockIndexEntry &text_block_index_entry) -> bool {
      return (cursor_position >= text_block_index_entry.start_position &&
              cursor_position < text_block_index_entry.end_position);
    }
  );
  // clang-format on

  if (text_block_index_it == d->text_block_index.end()) {
    return std::nullopt;
  }

  return text_block_index_it->index;
}

void CodeView::OnTextEditViewportMouseMoveEvent(QMouseEvent *event) {
  if (!isSignalConnected(QMetaMethod::fromSignal(&CodeView::TokenHovered))) {
    return;
  }

  auto opt_model_index = ModelIndexFromMousePosition(event->pos());
  if (!opt_model_index.has_value()) {
    d->opt_prev_hovered_model_index = std::nullopt;
    return;
  }

  const auto &model_index = opt_model_index.value();

  if (d->opt_prev_hovered_model_index.has_value()) {
    const auto &prev_hovered_model_index =
        d->opt_prev_hovered_model_index.value();

    if (prev_hovered_model_index.row == model_index.row &&
        prev_hovered_model_index.token_index == model_index.token_index) {
      return;
    }
  }

  d->opt_prev_hovered_model_index = model_index;
  emit TokenHovered(model_index);
}

void CodeView::OnTextEditViewportMouseButtonEvent(QMouseEvent *event,
                                                   bool double_click) {
  auto opt_model_index = ModelIndexFromMousePosition(event->pos());
  if (!opt_model_index.has_value()) {
    return;
  }

  const auto &model_index = opt_model_index.value();
  emit TokenClicked(model_index, event->buttons(), double_click);
}

void CodeView::OnModelReset() {
  auto palette = d->text_edit->palette();
  palette.setColor(QPalette::Window, d->theme.default_background_color);
  palette.setColor(QPalette::WindowText, d->theme.default_foreground_color);

  palette.setColor(QPalette::Base, d->theme.default_background_color);
  palette.setColor(QPalette::Text, d->theme.default_foreground_color);

  palette.setColor(QPalette::AlternateBase, d->theme.default_background_color);
  d->text_edit->setPalette(palette);

  d->text_edit->clear();
  d->text_block_index.clear();

  auto document = new QTextDocument(this);
  auto document_layout = new QPlainTextDocumentLayout(document);
  document->setDocumentLayout(document_layout);
  d->text_edit->setDocument(document);

  auto L_updateTextFormat = [&](QTextCharFormat &text_format,
                                const QVariant &token_category_var) {
    auto background_color =
        GetTextBackgroundColor(d->theme, token_category_var);

    text_format.setBackground(background_color);

    auto foreground_color =
        GetTextForegroundColor(d->theme, token_category_var);

    text_format.setForeground(foreground_color);

    auto text_style = GetTextStyle(d->theme, token_category_var);
    text_format.setFontItalic(text_style.italic);
    text_format.setFontWeight(text_style.bold ? QFont::DemiBold
                                              : QFont::Normal);

    text_format.setFontUnderline(text_style.underline);
    text_format.setFontStrikeOut(text_style.strikeout);
  };

  auto cursor = std::make_unique<QTextCursor>(document);
  auto row_count = d->model->RowCount();

  QProgressDialog progress(tr("Generating rows..."), tr("Abort"), 0, row_count,
                           this);
  progress.setWindowModality(Qt::WindowModal);

  CodeModelIndex model_index;
  QTextCharFormat text_format;

  cursor->beginEditBlock();

  for (int row_index = 0; row_index < row_count && !progress.wasCanceled();
       ++row_index) {
    model_index.row = row_index;

    if ((row_index % 100) == 0) {
      progress.setValue(row_index);
    }

    auto token_count = d->model->TokenCount(row_index);
    for (int token_index = 0; token_index < token_count; ++token_index) {
      if (progress.wasCanceled()) {
        break;
      }

      model_index.token_index = token_index;
      const auto &token_var = d->model->Data(model_index, Qt::DisplayRole);
      if (!token_var.isValid()) {
        continue;
      }

      const auto &token = token_var.toString();

      TextBlockIndexEntry index_entry;
      index_entry.start_position = cursor->position();
      index_entry.end_position =
          index_entry.start_position + static_cast<int>(token.size());

      index_entry.index = {row_index, token_index};
      d->text_block_index.push_back(std::move(index_entry));

      const auto &index = d->text_block_index.back().index;
      auto token_category_var =
          d->model->Data(index, ICodeModel::TokenCategoryRole);

      L_updateTextFormat(text_format, token_category_var);
      cursor->insertText(token, text_format);
    }

    cursor->insertText("\n");
  }

  cursor->endEditBlock();

  if (!progress.wasCanceled()) {
    progress.setValue(100);
  }

  d->gutter->setMinimumWidth(100);
}

void CodeView::OnTextEditViewportMouseButtonReleaseEvent(QMouseEvent *event) {
  OnTextEditViewportMouseButtonEvent(event, false);
}

void CodeView::OnTextEditViewportMouseButtonDblClick(QMouseEvent *event) {
  OnTextEditViewportMouseButtonEvent(event, true);
}

void CodeView::OnCursorPositionChange() {
  // TODO(alessandro): This should be part of the theme
  QTextEdit::ExtraSelection selection;
  selection.format.setBackground(QColor(Qt::black));
  selection.format.setProperty(QTextFormat::FullWidthSelection, true);
  selection.cursor = d->text_edit->textCursor();
  selection.cursor.clearSelection();

  d->text_edit->setExtraSelections({std::move(selection)});
}

void CodeView::OnGutterPaintEvent(QPaintEvent *event) {
  QPainter painter(d->gutter);

  const auto &base_color = d->text_edit->palette().base();
  painter.fillRect(event->rect(), base_color);
}

void CodeView::OnTextEditUpdateRequest(const QRect &, int) {
  d->gutter->update();
}

}  // namespace mx::gui
