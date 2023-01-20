/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "CodeView.h"
#include "SearchWidget.h"
#include "DefaultCodeViewThemes.h"

#include <QFont>
#include <QVBoxLayout>
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
#include <QShortcut>
#include <QFontMetrics>
#include <QWheelEvent>

#include <unordered_map>
#include <vector>
#include <optional>
#include <unordered_set>

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
using FileTokenIdToTextBlockIndexEntry =
    std::unordered_map<RawEntityId, std::size_t>;

using UniqueColumnIdToTextBlockIndexEntry =
    std::unordered_map<std::uint64_t, std::size_t>;

struct TokenGroupEntry final {
  int start_position{};
  int end_position{};
  CodeModelIndex index;
};

using TokenGroupMap =
    std::unordered_map<std::uint64_t, std::vector<TokenGroupEntry>>;

std::uint64_t CodeModelIndexToInteger(const CodeModelIndex &index) {
  return (static_cast<std::uint64_t>(index.row) << 32) |
         static_cast<std::uint64_t>(index.token_index);
}

std::optional<QColor> GetTextColorMapEntry(
    const QVariant &token_category_var,
    const std::unordered_map<TokenCategory, QColor> &color_map) {

  if (!token_category_var.isValid()) {
    return std::nullopt;
  }

  const auto &token_category =
      static_cast<TokenCategory>(token_category_var.toUInt());

  auto it = color_map.find(token_category);
  if (it == color_map.end()) {
    return std::nullopt;

  } else {
    return it->second;
  }
}

std::optional<QColor>
GetTextBackgroundColor(const CodeViewTheme &code_theme,
                       const QVariant &token_category_var) {
  return GetTextColorMapEntry(token_category_var,
                              code_theme.token_background_color_map);
}

QColor GetTextForegroundColor(const CodeViewTheme &code_theme,
                              const QVariant &token_category_var) {
  auto opt_color = GetTextColorMapEntry(token_category_var,
                                        code_theme.token_foreground_color_map);

  return opt_color.value_or(code_theme.default_foreground_color);
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

bool GetCodeModelIndexFromCursorPosition(CodeModelIndex &code_model_index,
                                         const TextBlockIndex &text_block_index,
                                         int cursor_position) {
  // clang-format off
  auto text_block_index_it = std::find_if(
    text_block_index.begin(),
    text_block_index.end(),

    [&](const TextBlockIndexEntry &text_block_index_entry) -> bool {
      return (cursor_position >= text_block_index_entry.start_position &&
              cursor_position <= text_block_index_entry.end_position);
    }
  );
  // clang-format on

  if (text_block_index_it == text_block_index.end()) {
    return false;
  }

  code_model_index = text_block_index_it->index;
  return true;
}

}  // namespace

struct CodeView::PrivateData final {
  ICodeModel *model{nullptr};

  QPlainTextEditMod *text_edit{nullptr};
  QWidget *gutter{nullptr};
  SearchWidget *search_widget{nullptr};

  TextBlockIndex text_block_index;
  FileTokenIdToTextBlockIndexEntry file_token_to_text_block;
  UniqueColumnIdToTextBlockIndexEntry column_id_to_text_block;
  TokenGroupMap token_group_map;
  std::size_t highest_line_number{};

  CodeViewTheme theme;
  std::size_t tab_width{4};

  std::optional<CodeModelIndex> opt_prev_hovered_model_index;
};

void CodeView::SetTheme(const CodeViewTheme &theme) {
  d->theme = theme;

  QFont font(d->theme.font_name);
  font.setStyleHint(QFont::TypeWriter);
  setFont(font);

  OnModelReset();
}

void CodeView::SetTabWidth(std::size_t width) {
  d->tab_width = width;

  UpdateTabStopDistance();
}

CodeView::~CodeView() {}

std::optional<int>
CodeView::GetFileTokenCursorPosition(RawEntityId file_token_id) const {
  if (!IsValidFileToken(file_token_id)) {
    return std::nullopt;
  }

  auto it = d->file_token_to_text_block.find(file_token_id);
  if (it == d->file_token_to_text_block.end()) {
    return std::nullopt;
  }

  const auto &text_block_index_elem = it->second;

  const auto &text_block_index_entry =
      d->text_block_index.at(text_block_index_elem);

  return text_block_index_entry.start_position;
}

std::optional<int> CodeView::GetTokenCursorPosition(const Token &token) const {
  if (!token) {
    return std::nullopt;
  }

  return GetFileTokenCursorPosition(token.id().Pack());
}

std::optional<int> CodeView::GetStartTokenRangeCursorPosition(
    const TokenRange &token_range) const {
  if (!token_range) {
    return std::nullopt;
  }

  return GetFileTokenCursorPosition(token_range[0].id().Pack());
}

int CodeView::GetCursorPosition() const {
  auto text_cursor = d->text_edit->textCursor();
  return text_cursor.position();
}

bool CodeView::SetCursorPosition(int start, std::optional<int> opt_end) const {
  auto text_cursor = d->text_edit->textCursor();
  text_cursor.setPosition(start, QTextCursor::MoveMode::MoveAnchor);

  d->text_edit->moveCursor(QTextCursor::End);
  d->text_edit->setTextCursor(text_cursor);
  d->text_edit->centerCursor();

  if (opt_end.has_value()) {
    text_cursor.setPosition(opt_end.value(), QTextCursor::MoveMode::KeepAnchor);
    d->text_edit->setTextCursor(text_cursor);
  }

  return true;
}

QString CodeView::Text() const {
  return d->text_edit->toPlainText();
}

void CodeView::SetWordWrapping(bool enabled) {
  d->text_edit->setWordWrapMode(enabled ? QTextOption::WordWrap
                                        : QTextOption::NoWrap);
}

bool CodeView::ScrollToFileToken(RawEntityId file_token_id) const {
  auto opt_token_pos = GetFileTokenCursorPosition(file_token_id);
  if (!opt_token_pos.has_value()) {
    return false;
  }

  const auto &token_pos = opt_token_pos.value();
  return SetCursorPosition(token_pos, std::nullopt);
}

bool CodeView::ScrollToToken(const Token &token) const {
  if (!token) {
    return false;
  }

  return ScrollToFileToken(token.id().Pack());
}

bool CodeView::ScrollToTokenRange(const TokenRange &token_range) const {
  if (!token_range) {
    return false;
  }

  return ScrollToFileToken(token_range[0].id().Pack());
}

CodeView::CodeView(ICodeModel *model, QWidget *parent)
    : ICodeView(parent),
      d(new PrivateData) {

  InstallModel(model);
  InitializeWidgets();

  SetWordWrapping(false);
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

  } else if (event->type() == QEvent::Wheel) {
    auto wheel_event = static_cast<QWheelEvent *>(event);

    if (obj == d->text_edit->viewport() &&
        (wheel_event->modifiers() & Qt::ControlModifier) != 0) {

      OnTextEditTextZoom(wheel_event);
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
  // Code viewer

  d->text_edit = new QPlainTextEditMod();
  d->text_edit->setReadOnly(true);
  d->text_edit->setTextInteractionFlags(Qt::TextBrowserInteraction);
  d->text_edit->viewport()->installEventFilter(this);
  d->text_edit->viewport()->setMouseTracking(true);

  connect(d->text_edit, &QPlainTextEditMod::updateRequest, this,
          &CodeView::OnTextEditUpdateRequest);

  // Gutter
  d->gutter = new QWidget();
  d->gutter->installEventFilter(this);

  // Search widget
  d->search_widget = new SearchWidget(this);
  d->search_widget->Activate();

  auto enable_search_shortcut = new QShortcut(this);
  enable_search_shortcut->setKey(QKeySequence("Ctrl+F"));
  connect(enable_search_shortcut, &QShortcut::activated, d->search_widget,
          &SearchWidget::Activate);

  auto search_previous_shortcut = new QShortcut(this);
  search_previous_shortcut->setKey(QKeySequence("Shift+F3"));
  connect(search_previous_shortcut, &QShortcut::activated, d->search_widget,
          &SearchWidget::OnShowPrevResult);

  auto search_next_shortcut = new QShortcut(this);
  search_next_shortcut->setKey(QKeySequence("F3"));
  connect(search_next_shortcut, &QShortcut::activated, d->search_widget,
          &SearchWidget::OnShowNextResult);

  auto disable_search_shortcut = new QShortcut(this);
  disable_search_shortcut->setKey(QKeySequence("Escape"));
  connect(disable_search_shortcut, &QShortcut::activated, d->search_widget,
          &SearchWidget::Deactivate);

  // Layout for the gutter and code view
  auto code_layout = new QHBoxLayout();
  code_layout->setContentsMargins(0, 0, 0, 0);
  code_layout->setSpacing(0);
  code_layout->addWidget(d->gutter);
  code_layout->addWidget(d->text_edit);

  // Main layout
  auto main_layout = new QVBoxLayout();
  main_layout->setContentsMargins(0, 0, 0, 0);
  main_layout->setSpacing(0);
  main_layout->addItem(code_layout);
  main_layout->addWidget(d->search_widget);
  setLayout(main_layout);

  // This will also cause a model reset update
  SetTheme(kDefaultDarkCodeViewTheme);

  // Apply the default tab stop distance
  SetTabWidth(4);
}

bool CodeView::IsValidFileToken(RawEntityId file_token_id) const {
  if (file_token_id == kInvalidEntityId) {
    return false;
  }

  const auto &variant_id = EntityId(file_token_id).Unpack();
  return std::holds_alternative<FileTokenId>(variant_id);
}

std::optional<CodeModelIndex>
CodeView::ModelIndexFromMousePosition(QPoint pos) {
  auto text_cursor = d->text_edit->cursorForPosition(pos);
  auto cursor_position = text_cursor.position();

  CodeModelIndex code_model_index{};
  if (!GetCodeModelIndexFromCursorPosition(
          code_model_index, d->text_block_index, cursor_position)) {
    return std::nullopt;
  }

  return code_model_index;
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

void CodeView::OnTextEditTextZoom(QWheelEvent *event) {
  qreal delta = static_cast<qreal>(event->angleDelta().y()) / 120.0;
  if (delta == 0.0) {
    return;
  }

  auto font = d->text_edit->font();

  qreal point_size = font.pointSizeF() + delta;
  if (point_size <= 0.0) {
    return;
  }

  font.setPointSizeF(point_size);
  setFont(font);

  UpdateTabStopDistance();
  UpdateGutterWidth();
}

void CodeView::UpdateTabStopDistance() {
  QFontMetricsF font_metrics(d->text_edit->font());

  auto base_width = font_metrics.horizontalAdvance(QChar::VisualTabCharacter);
  d->text_edit->setTabStopDistance(base_width *
                                   static_cast<qreal>(d->tab_width));
}

void CodeView::UpdateGutterWidth() {
  auto required_gutter_width = fontMetrics().horizontalAdvance(
      QString::number(d->highest_line_number) + "000");

  d->gutter->setMinimumWidth(required_gutter_width);
}

void CodeView::UpdateTokenGroupColors() {
  // TODO(alessandro): These should be either derived from the
  // active palette or taken from the theme.
  static std::vector<QColor> kColorMap{
      QColor::fromRgb(0x101119), QColor::fromRgb(0x0F171A),
      QColor::fromRgb(0x060E0B), QColor::fromRgb(0x1C1A0C),
      QColor::fromRgb(0x221607), QColor::fromRgb(0x291500),
      QColor::fromRgb(0x2B1412), QColor::fromRgb(0x2B121D),
      QColor::fromRgb(0x2C1125), QColor::fromRgb(0x1E0B1E),
  };

  std::size_t color_map_index{};
  QList<QTextEdit::ExtraSelection> extra_selection_list;
  QTextEdit::ExtraSelection selection;

  std::unordered_set<std::uint64_t> visited_model_index_list;
  std::unordered_set<int> colored_line_list;

  for (const auto &token_group_map_p : d->token_group_map) {
    const auto &token_group = token_group_map_p.second;

    const auto &group_color = kColorMap.at(color_map_index);
    ++color_map_index;

    for (const auto &token : token_group) {
      auto unique_column_id = CodeModelIndexToInteger(token.index);
      visited_model_index_list.insert(unique_column_id);

      selection = {};
      selection.format.setBackground(group_color);
      selection.cursor = d->text_edit->textCursor();
      selection.cursor.setPosition(token.start_position,
                                   QTextCursor::MoveMode::MoveAnchor);

      selection.cursor.setPosition(token.end_position,
                                   QTextCursor::MoveMode::KeepAnchor);

      extra_selection_list.append(std::move(selection));

      auto column_count = d->model->TokenCount(token.index.row);
      auto is_last = token.index.token_index + 1 == column_count;

      if (is_last) {
        colored_line_list.insert(token.index.row);

        selection = {};
        selection.format.setBackground(group_color);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = d->text_edit->textCursor();
        selection.cursor.setPosition(token.start_position,
                                     QTextCursor::MoveMode::MoveAnchor);

        selection.cursor.clearSelection();
        extra_selection_list.prepend(std::move(selection));
      }
    }
  }

  for (const auto &colored_line : colored_line_list) {
    auto column_count = d->model->TokenCount(colored_line);

    for (int column = 0; column < column_count; ++column) {
      auto unique_column_id = CodeModelIndexToInteger({colored_line, column});
      if (visited_model_index_list.count(unique_column_id) > 0) {
        continue;
      }

      auto column_id_to_text_block_it =
          d->column_id_to_text_block.find(unique_column_id);
      if (column_id_to_text_block_it == d->column_id_to_text_block.end()) {
        continue;
      }

      const auto &text_block_index_elem = column_id_to_text_block_it->second;
      const auto &text_block_index_entry =
          d->text_block_index.at(text_block_index_elem);

      selection = {};
      selection.format.setBackground(d->theme.default_background_color);
      selection.cursor = d->text_edit->textCursor();
      selection.cursor.setPosition(text_block_index_entry.start_position,
                                   QTextCursor::MoveMode::MoveAnchor);

      selection.cursor.setPosition(text_block_index_entry.end_position,
                                   QTextCursor::MoveMode::KeepAnchor);

      extra_selection_list.append(std::move(selection));
    }
  }

  d->text_edit->setExtraSelections(std::move(extra_selection_list));
}

void CodeView::OnModelReset() {
  d->search_widget->Deactivate();

  auto palette = d->text_edit->palette();
  palette.setColor(QPalette::Window, d->theme.default_background_color);
  palette.setColor(QPalette::WindowText, d->theme.default_foreground_color);

  palette.setColor(QPalette::Base, d->theme.default_background_color);
  palette.setColor(QPalette::Text, d->theme.default_foreground_color);

  palette.setColor(QPalette::AlternateBase, d->theme.default_background_color);
  d->text_edit->setPalette(palette);

  d->text_edit->clear();
  d->text_block_index.clear();
  d->file_token_to_text_block.clear();
  d->column_id_to_text_block.clear();
  d->highest_line_number = 0;

  auto document = new QTextDocument(this);
  document->setDefaultFont(font());

  auto document_layout = new QPlainTextDocumentLayout(document);
  document->setDocumentLayout(document_layout);

  d->text_edit->setDocument(document);

  auto L_updateTextFormat = [&](QTextCharFormat &text_format,
                                const QVariant &token_category_var) {
    auto opt_background_color =
        GetTextBackgroundColor(d->theme, token_category_var);

    if (opt_background_color.has_value()) {
      text_format.setBackground(opt_background_color.value());
    }

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

  std::size_t highest_line_number{};

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

      const auto &line_number_var =
          d->model->Data(model_index, ICodeModel::LineNumberRole);

      if (line_number_var.isValid()) {
        auto line_number =
            static_cast<std::size_t>(line_number_var.toULongLong());

        highest_line_number = std::max(line_number, highest_line_number);
      }

      const auto &token = token_var.toString();

      const auto &token_id_var =
          d->model->Data(model_index, ICodeModel::TokenRawEntityIdRole);

      if (!token_id_var.isValid()) {
        continue;
      }

      auto file_token_id = static_cast<RawEntityId>(token_id_var.toULongLong());

      // TODO(alessandro): This index is used for things like ScrollToToken. When
      // we split tokens however, we could end up with two different tokens using
      // the same id. This will make this insertion fail, breaking the lookup
      // functionality for some tokens.
      d->file_token_to_text_block.insert(
          {file_token_id, d->text_block_index.size()});

      auto unique_column_id = CodeModelIndexToInteger(model_index);
      d->column_id_to_text_block.insert(
          {unique_column_id, d->text_block_index.size()});

      TextBlockIndexEntry index_entry;
      index_entry.start_position = cursor->position();
      index_entry.end_position =
          index_entry.start_position + static_cast<int>(token.size());

      index_entry.index = {row_index, token_index};

      const auto &token_group_id_var =
          d->model->Data(model_index, ICodeModel::TokenGroupIdRole);

      if (token_group_id_var.isValid()) {
        auto token_group_id =
            static_cast<std::uint64_t>(token_group_id_var.toULongLong());

        auto token_group_map_it = d->token_group_map.find(token_group_id);
        if (token_group_map_it == d->token_group_map.end()) {
          auto insert_status = d->token_group_map.insert({token_group_id, {}});
          token_group_map_it = insert_status.first;
        }

        auto &token_group = token_group_map_it->second;
        token_group.push_back(TokenGroupEntry{index_entry.start_position,
                                              index_entry.end_position,
                                              index_entry.index});
      }

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

  d->highest_line_number = highest_line_number;

  UpdateGutterWidth();
  UpdateTokenGroupColors();
}

void CodeView::OnTextEditViewportMouseButtonReleaseEvent(QMouseEvent *event) {
  OnTextEditViewportMouseButtonEvent(event, false);
}

void CodeView::OnTextEditViewportMouseButtonDblClick(QMouseEvent *event) {
  OnTextEditViewportMouseButtonEvent(event, true);
}

void CodeView::OnGutterPaintEvent(QPaintEvent *event) {
  QPainter painter(d->gutter);
  painter.fillRect(event->rect(), d->theme.default_gutter_background);

  painter.setPen(d->theme.default_gutter_foreground);
  painter.setFont(font());

  QTextBlock block = d->text_edit->firstVisibleBlock();
  int top = qRound(d->text_edit->blockBoundingGeometry(block)
                       .translated(d->text_edit->contentOffset())
                       .top());

  int bottom = top + qRound(d->text_edit->blockBoundingRect(block).height());

  CodeModelIndex code_model_index{};

  auto right_line_num_margin = d->gutter->width() - (d->gutter->width() / 3);

  while (block.isValid() && top <= event->rect().bottom()) {
    if (block.isVisible() && bottom >= event->rect().top()) {
      auto succeeded = GetCodeModelIndexFromCursorPosition(
          code_model_index, d->text_block_index, block.position());

      if (succeeded) {
        auto line_number_var =
            d->model->Data(code_model_index, ICodeModel::LineNumberRole);

        if (line_number_var.isValid()) {
          auto line_number =
              static_cast<std::size_t>(line_number_var.toULongLong());

          painter.drawText(0, top, right_line_num_margin,
                           fontMetrics().height(), Qt::AlignRight,
                           QString::number(line_number));
        }
      }
    }

    block = block.next();
    top = bottom;
    bottom = top + qRound(d->text_edit->blockBoundingRect(block).height());
  }
}

void CodeView::OnTextEditUpdateRequest(const QRect &rect, int dy) {
  if (dy) {
    d->gutter->scroll(0, dy);
  } else {
    d->gutter->update(0, rect.y(), d->gutter->width(), rect.height());
  }
}

}  // namespace mx::gui
