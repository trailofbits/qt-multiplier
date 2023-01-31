/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "CodeView.h"
#include "InternalSearchWidget.h"
#include "DefaultCodeViewThemes.h"

#include <QFont>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMetaMethod>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPlainTextDocumentLayout>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QWidget>
#include <QProgressDialog>
#include <QShortcut>
#include <QFontMetrics>
#include <QWheelEvent>

#include <unordered_set>

namespace mx::gui {

namespace {

class QPlainTextEditMod final : public QPlainTextEdit {
 public:
  QPlainTextEditMod(QWidget *parent = nullptr) : QPlainTextEdit(parent) {}

  virtual ~QPlainTextEditMod() override{};

  friend class mx::gui::CodeView;
};

}  // namespace

struct CodeView::PrivateData final {
  ICodeModel *model{nullptr};

  QPlainTextEditMod *text_edit{nullptr};
  QWidget *gutter{nullptr};
  InternalSearchWidget *search_widget{nullptr};

  TokenMap token_map;

  CodeViewTheme theme;
  std::size_t tab_width{4};

  std::optional<CodeModelIndex> opt_prev_hovered_model_index;

  QShortcut *enable_search_shortcut{nullptr};
  QShortcut *disable_search_shortcut{nullptr};
  QShortcut *search_previous_shortcut{nullptr};
  QShortcut *search_next_shortcut{nullptr};
};

void CodeView::SetTheme(const CodeViewTheme &theme) {
  d->theme = theme;

  QFont font(d->theme.font_name);
  font.setStyleHint(QFont::TypeWriter);
  setFont(font);

  auto palette = d->text_edit->palette();
  palette.setColor(QPalette::Window, d->theme.default_background_color);
  palette.setColor(QPalette::WindowText, d->theme.default_foreground_color);

  palette.setColor(QPalette::Base, d->theme.default_background_color);
  palette.setColor(QPalette::Text, d->theme.default_foreground_color);

  palette.setColor(QPalette::AlternateBase, d->theme.default_background_color);
  d->text_edit->setPalette(palette);

  OnModelReset();
}

void CodeView::SetTabWidth(std::size_t width) {
  d->tab_width = width;
  UpdateTabStopDistance();
}

CodeView::~CodeView() {}

std::optional<int>
CodeView::GetEntityCursorPosition(RawEntityId entity_id) const {
  if (entity_id == kInvalidEntityId) {
    return std::nullopt;
  }

  auto unique_token_id_list_it =
      d->token_map.entity_id_to_unique_token_id_list.find(entity_id);
  if (unique_token_id_list_it ==
      d->token_map.entity_id_to_unique_token_id_list.end()) {
    return std::nullopt;
  }

  const auto &unique_token_id_list = unique_token_id_list_it->second;
  return unique_token_id_list.front();
}

int CodeView::GetCursorPosition() const {
  auto text_cursor = d->text_edit->textCursor();
  return text_cursor.position();
}

bool CodeView::SetCursorPosition(int start, std::optional<int> opt_end) const {

  auto text_cursor = d->text_edit->textCursor();
  text_cursor.movePosition(QTextCursor::End);

  auto max_position = text_cursor.position();
  if (start >= max_position || opt_end.value_or(start) >= max_position) {
    return false;
  }

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

bool CodeView::ScrollToEntityId(RawEntityId entity_id) const {
  auto opt_token_pos = GetEntityCursorPosition(entity_id);
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

  return ScrollToEntityId(token.id().Pack());
}

bool CodeView::ScrollToTokenRange(const TokenRange &token_range) const {
  if (!token_range) {
    return false;
  }

  return ScrollToEntityId(token_range[0].id().Pack());
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
  d->search_widget = new InternalSearchWidget(this);
  d->search_widget->Activate();

  d->enable_search_shortcut = new QShortcut(
      QKeySequence::Find, this, d->search_widget,
      &InternalSearchWidget::Activate, Qt::WidgetWithChildrenShortcut);

  d->disable_search_shortcut = new QShortcut(
      QKeySequence::Cancel, this, d->search_widget,
      &InternalSearchWidget::Deactivate, Qt::WidgetWithChildrenShortcut);

  d->search_previous_shortcut = new QShortcut(
      QKeySequence::FindPrevious, this, d->search_widget,
      &InternalSearchWidget::OnShowPrevResult, Qt::WidgetWithChildrenShortcut);

  d->search_next_shortcut = new QShortcut(
      QKeySequence::FindNext, this, d->search_widget,
      &InternalSearchWidget::OnShowNextResult, Qt::WidgetWithChildrenShortcut);

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

std::optional<CodeModelIndex>
CodeView::GetCodeModelIndexFromMousePosition(const QPoint &pos) {
  auto text_cursor = d->text_edit->cursorForPosition(pos);
  return GetCodeModelIndexFromTextCursor(d->token_map, text_cursor);
}

void CodeView::OnTextEditViewportMouseMoveEvent(QMouseEvent *event) {
  if (!isSignalConnected(QMetaMethod::fromSignal(&CodeView::TokenHovered))) {
    return;
  }

  auto opt_model_index = GetCodeModelIndexFromMousePosition(event->pos());
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
  auto opt_model_index = GetCodeModelIndexFromMousePosition(event->pos());
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
  const auto &font_metrics = fontMetrics();

  auto gutter_margin = font_metrics.horizontalAdvance("0") * 4;
  auto required_gutter_width = font_metrics.horizontalAdvance(
      QString::number(d->token_map.highest_line_number));

  d->gutter->setMinimumWidth(gutter_margin + required_gutter_width);
}

void CodeView::UpdateTokenGroupColors() {
  auto extra_selection_list =
      GenerateExtraSelections(d->token_map, *d->text_edit, *d->model, d->theme);

  // TODO: Implement here any additional highlighting by appending
  // into the extra selection list

  d->text_edit->setExtraSelections(std::move(extra_selection_list));
}

std::uint64_t CodeView::GetUniqueTokenIdentifier(const CodeModelIndex &index) {

  return (static_cast<std::uint64_t>(index.row) << 32) |
         static_cast<std::uint64_t>(index.token_index);
}

std::optional<std::size_t>
CodeView::GetLineNumberFromBlockNumber(const TokenMap &token_map,
                                       int block_number) {

  auto line_number_it =
      token_map.block_number_to_line_number.find(block_number);
  if (line_number_it == token_map.block_number_to_line_number.end()) {
    return std::nullopt;
  }

  return line_number_it->second;
}

std::optional<CodeModelIndex>
CodeView::GetCodeModelIndexFromTextCursor(const TokenMap &token_map,
                                          const QTextCursor &cursor) {

  auto line_number = cursor.blockNumber();

  auto token_unique_id_list_it =
      token_map.block_number_to_unique_token_id_list.find(line_number);

  if (token_unique_id_list_it ==
      token_map.block_number_to_unique_token_id_list.end()) {

    return std::nullopt;
  }

  const auto &token_unique_id_list = token_unique_id_list_it->second;

  auto cursor_position = cursor.position();
  for (const auto &token_unique_id : token_unique_id_list) {
    const auto &token_map_entry = token_map.data.at(token_unique_id);
    if (cursor_position > token_map_entry.cursor_end) {
      break;
    }

    if (cursor_position >= token_map_entry.cursor_start &&
        cursor_position <= token_map_entry.cursor_end) {

      return token_map_entry.model_index;
    }
  }

  return std::nullopt;
}

std::vector<int>
CodeView::GetTextCursorListFromRawEntityId(const TokenMap &token_map,
                                           const RawEntityId &entity_id) {

  auto token_unique_id_list_it =
      token_map.entity_id_to_unique_token_id_list.find(entity_id);

  if (token_unique_id_list_it ==
      token_map.entity_id_to_unique_token_id_list.end()) {
    return {};
  }

  const auto &token_unique_id_list = token_unique_id_list_it->second;

  std::vector<int> cursor_list;

  for (const auto &token_unique_id : token_unique_id_list) {
    const auto &token_map_entry = token_map.data.at(token_unique_id);
    cursor_list.push_back(token_map_entry.cursor_start);
  }

  return cursor_list;
}

QTextDocument *CodeView::CreateTextDocument(
    CodeView::TokenMap &token_map, const ICodeModel &model,
    const CodeViewTheme &theme,
    const std::optional<CreateTextDocumentProgressCallback>
        &opt_progress_callback) {

  token_map = {};

  auto document = new QTextDocument();
  auto document_layout = new QPlainTextDocumentLayout(document);
  document->setDocumentLayout(document_layout);

  auto row_count = model.RowCount();
  if (row_count == 0) {
    return document;
  }

  QTextCursor cursor(document);
  cursor.beginEditBlock();

  auto L_updateProgress = [&](int current_row) -> bool {
    if (!opt_progress_callback.has_value()) {
      return true;
    }

    const auto &progress_callback = opt_progress_callback.value();

    if (current_row == row_count) {
      static_cast<void>(progress_callback(100));
      return true;
    }

    if ((current_row % 100) != 0) {
      return true;
    }

    auto current_progress = (current_row * 100) / row_count;
    return progress_callback(current_progress);
  };

  for (int row_index = 0; row_index < row_count; ++row_index) {
    if (!L_updateProgress(row_index)) {
      break;
    }

    CodeModelIndex model_index = {row_index, 0};
    auto line_mappings_need_update{true};
    auto token_count = model.TokenCount(row_index);

    for (int token_index = 0; token_index < token_count; ++token_index) {
      // Update the highest line number value
      const auto &line_number_var =
          model.Data(model_index, ICodeModel::LineNumberRole);

      if (line_number_var.isValid()) {
        auto line_number =
            static_cast<std::size_t>(line_number_var.toULongLong());

        token_map.highest_line_number =
            std::max(line_number, token_map.highest_line_number);

        if (line_mappings_need_update) {
          auto block_number = cursor.blockNumber();

          token_map.line_number_to_block_number.insert(
              {line_number, block_number});

          token_map.block_number_to_line_number.insert(
              {block_number, line_number});

          line_mappings_need_update = false;
        }
      }

      // Get the token that will have to be displayed on screen. There is nothing
      // else to do here if it is not visible
      model_index.token_index = token_index;
      const auto &token_var = model.Data(model_index, Qt::DisplayRole);
      if (!token_var.isValid()) {
        continue;
      }

      const auto &token = token_var.toString();
      if (token.isEmpty()) {
        continue;
      }

      // Generate the token map entry
      auto unique_token_id = GetUniqueTokenIdentifier(model_index);

      TokenMap::Entry entry{};
      entry.cursor_start = cursor.position();
      entry.cursor_end = entry.cursor_start + static_cast<int>(token.size());
      entry.model_index = model_index;

      token_map.data.insert({unique_token_id, std::move(entry)});

      // Add the entry to the RawEntityId index
      const auto &entity_id_var =
          model.Data(model_index, ICodeModel::TokenRawEntityIdRole);

      if (entity_id_var.isValid()) {
        auto entity_id = static_cast<RawEntityId>(entity_id_var.toULongLong());
        if (entity_id != kInvalidEntityId) {
          auto unique_token_id_list_it =
              token_map.entity_id_to_unique_token_id_list.find(entity_id);
          if (unique_token_id_list_it ==
              token_map.entity_id_to_unique_token_id_list.end()) {
            auto insert_status =
                token_map.entity_id_to_unique_token_id_list.insert(
                    {entity_id, {}});
            unique_token_id_list_it = insert_status.first;
          }

          auto &unique_token_id_list = unique_token_id_list_it->second;
          unique_token_id_list.push_back(unique_token_id);
        }
      }

      // Add the entry to the block number index
      {
        auto block_number = cursor.blockNumber();

        auto unique_token_id_list_it =
            token_map.block_number_to_unique_token_id_list.find(block_number);

        if (unique_token_id_list_it ==
            token_map.block_number_to_unique_token_id_list.end()) {

          auto insert_status =
              token_map.block_number_to_unique_token_id_list.insert(
                  {block_number, {}});

          unique_token_id_list_it = insert_status.first;
        }

        auto &unique_token_id_list = unique_token_id_list_it->second;
        unique_token_id_list.push_back(unique_token_id);
      }

      // Add the entry to the token group index
      const auto &token_group_id_var =
          model.Data(model_index, ICodeModel::TokenGroupIdRole);

      if (token_group_id_var.isValid()) {
        auto token_group_id =
            static_cast<std::uint64_t>(token_group_id_var.toULongLong());

        auto unique_token_id_list_it =
            token_map.token_group_id_to_unique_token_id_list.find(
                token_group_id);

        if (unique_token_id_list_it ==
            token_map.token_group_id_to_unique_token_id_list.end()) {

          auto insert_status =
              token_map.token_group_id_to_unique_token_id_list.insert(
                  {token_group_id, {}});

          unique_token_id_list_it = insert_status.first;
        }

        auto &unique_token_id_list = unique_token_id_list_it->second;
        unique_token_id_list.push_back(unique_token_id);
      }

      // Add the token to the document
      auto token_category_var =
          model.Data(model_index, ICodeModel::TokenCategoryRole);

      QTextCharFormat text_format;
      ConfigureTextFormatFromTheme(text_format, theme, token_category_var);

      cursor.insertText(token, text_format);
    }

    cursor.insertText("\n");
  }

  cursor.endEditBlock();

  L_updateProgress(row_count);
  return document;
}

std::optional<QColor> CodeView::GetTextColorMapEntryFromTheme(
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
CodeView::GetTextBackgroundColorFromTheme(const CodeViewTheme &theme,
                                          const QVariant &token_category_var) {

  return GetTextColorMapEntryFromTheme(token_category_var,
                                       theme.token_background_color_map);
}

QColor
CodeView::GetTextForegroundColorFromTheme(const CodeViewTheme &theme,
                                          const QVariant &token_category_var) {

  auto opt_color = GetTextColorMapEntryFromTheme(
      token_category_var, theme.token_foreground_color_map);

  return opt_color.value_or(theme.default_foreground_color);
}

CodeViewTheme::Style
CodeView::GetTextStyleFromTheme(const CodeViewTheme &theme,
                                const QVariant &token_category_var) {
  if (!token_category_var.isValid()) {
    return {};
  }

  const auto &token_category =
      static_cast<TokenCategory>(token_category_var.toUInt());

  auto it = theme.token_style_map.find(token_category);
  if (it == theme.token_style_map.end()) {
    return {};

  } else {
    return it->second;
  }
}

void CodeView::ConfigureTextFormatFromTheme(
    QTextCharFormat &text_format, const CodeViewTheme &theme,
    const QVariant &token_category_var) {

  auto opt_background_color =
      GetTextBackgroundColorFromTheme(theme, token_category_var);
  if (opt_background_color.has_value()) {
    text_format.setBackground(opt_background_color.value());
  }

  auto foreground_color =
      GetTextForegroundColorFromTheme(theme, token_category_var);
  text_format.setForeground(foreground_color);

  auto text_style = GetTextStyleFromTheme(theme, token_category_var);
  text_format.setFontItalic(text_style.italic);
  text_format.setFontWeight(text_style.bold ? QFont::DemiBold : QFont::Normal);

  text_format.setFontUnderline(text_style.underline);
  text_format.setFontStrikeOut(text_style.strikeout);
}

QList<QTextEdit::ExtraSelection> CodeView::GenerateExtraSelections(
    const TokenMap &token_map, QPlainTextEdit &text_edit,
    const ICodeModel &model, const CodeViewTheme &theme) {

  QList<QTextEdit::ExtraSelection> extra_selection_list;

  std::size_t color_map_index{};
  QTextEdit::ExtraSelection selection;

  std::unordered_set<std::uint64_t> visited_model_index_list;
  std::unordered_set<int> colored_line_list;

  for (const auto &token_group_p :
       token_map.token_group_id_to_unique_token_id_list) {

    const auto &unique_token_id_list = token_group_p.second;

    const auto &group_color =
        theme.token_group_color_list[color_map_index %
                                     theme.token_group_color_list.size()];

    ++color_map_index;

    for (const auto &unique_token_id : unique_token_id_list) {
      const auto &token_map_entry = token_map.data.at(unique_token_id);
      visited_model_index_list.insert(unique_token_id);

      selection = {};
      selection.format.setBackground(group_color);
      selection.cursor = text_edit.textCursor();
      selection.cursor.setPosition(token_map_entry.cursor_start,
                                   QTextCursor::MoveMode::MoveAnchor);

      selection.cursor.setPosition(token_map_entry.cursor_end,
                                   QTextCursor::MoveMode::KeepAnchor);

      extra_selection_list.append(std::move(selection));

      auto column_count = model.TokenCount(token_map_entry.model_index.row);
      auto is_last =
          token_map_entry.model_index.token_index + 1 == column_count;

      if (is_last) {
        colored_line_list.insert(token_map_entry.model_index.row);

        selection = {};
        selection.format.setBackground(group_color);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = text_edit.textCursor();
        selection.cursor.setPosition(token_map_entry.cursor_end,
                                     QTextCursor::MoveMode::MoveAnchor);

        selection.cursor.clearSelection();
        extra_selection_list.prepend(std::move(selection));
      }
    }
  }

  for (const auto &colored_line : colored_line_list) {
    auto column_count = model.TokenCount(colored_line);

    for (int column = 0; column < column_count; ++column) {
      auto unique_token_id = GetUniqueTokenIdentifier({colored_line, column});
      if (visited_model_index_list.count(unique_token_id) > 0) {
        continue;
      }

      const auto &token_map_entry = token_map.data.at(unique_token_id);

      selection = {};
      selection.format.setBackground(theme.default_background_color);
      selection.cursor = text_edit.textCursor();
      selection.cursor.setPosition(token_map_entry.cursor_start,
                                   QTextCursor::MoveMode::MoveAnchor);

      selection.cursor.setPosition(token_map_entry.cursor_end,
                                   QTextCursor::MoveMode::KeepAnchor);

      extra_selection_list.append(std::move(selection));
    }
  }

  return extra_selection_list;
}

void CodeView::OnModelReset() {
  d->search_widget->Deactivate();

  QProgressDialog progress(tr("Generating rows..."), tr("Abort"), 0, 100, this);
  progress.setWindowModality(Qt::WindowModal);

  // clang-format off
  auto document = CreateTextDocument(
    d->token_map,
    *d->model,
    d->theme,

    [&progress](int current_progress) -> bool {
      if (progress.wasCanceled()) {
        return false;
      }

      progress.setValue(current_progress);
      return true;
    }
  );
  // clang-format on

  if (progress.wasCanceled()) {
    return;
  }

  document->setDefaultFont(d->text_edit->font());
  d->text_edit->setDocument(document);

  UpdateGutterWidth();
  UpdateTokenGroupColors();
  UpdateTabStopDistance();
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

  auto right_line_num_margin = d->gutter->width() - (d->gutter->width() / 3);

  while (block.isValid() && top <= event->rect().bottom()) {
    if (block.isVisible() && bottom >= event->rect().top()) {
      auto block_number = block.firstLineNumber();

      auto opt_line_number =
          GetLineNumberFromBlockNumber(d->token_map, block_number);

      if (opt_line_number.has_value()) {
        auto line_number = opt_line_number.value();
        painter.drawText(0, top, right_line_num_margin, fontMetrics().height(),
                         Qt::AlignRight, QString::number(line_number));
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
