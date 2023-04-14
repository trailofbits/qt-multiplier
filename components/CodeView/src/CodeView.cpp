/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "CodeView.h"
#include "GoToLineWidget.h"

#include <multiplier/ui/Assert.h>
#include <multiplier/ui/CodeViewTheme.h>

#include <multiplier/Entities/TokenCategory.h>

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
#include <QFontMetrics>
#include <QWheelEvent>
#include <QRegularExpression>
#include <QShortcut>
#include <QTimer>

#include <unordered_set>
#include <vector>

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

  int version{0};
  int last_press_version{-1};
  int last_press_position{-1};
  int last_block{-1};

  QPlainTextEditMod *text_edit{nullptr};
  QWidget *gutter{nullptr};

  QMetaObject::Connection cursor_change_signal;

  ISearchWidget *search_widget{nullptr};
  std::vector<std::pair<int, int>> search_result_list;

  TokenMap token_map;

  CodeViewTheme theme;
  std::size_t tab_width{4};

  std::optional<CodeModelIndex> opt_prev_hovered_model_index;

  QShortcut *go_to_line_shortcut{nullptr};
  GoToLineWidget *go_to_line_widget{nullptr};

  std::optional<unsigned> deferred_scroll_to_line;
};

ICodeModel *CodeView::Model() {
  return d->model;
}

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

int CodeView::GetCursorPosition() const {
  auto text_cursor = d->text_edit->textCursor();
  return text_cursor.position();
}

bool CodeView::SetCursorPosition(int start, std::optional<int> opt_end) {
  d->version++;

  auto text_cursor = d->text_edit->textCursor();
  text_cursor.movePosition(QTextCursor::End);

  auto max_position = text_cursor.position();
  if (start >= max_position || opt_end.value_or(start) >= max_position) {
    return false;
  }

  // NOTE(pag): We stop cursor tracking so that the individual cursor
  //            manipulations here that are needed to center the view on the
  //            cursor don't bubble up to higher levels.
  StopCursorTracking();

  text_cursor.setPosition(start, QTextCursor::MoveMode::MoveAnchor);

  // We want to change the scroll in the viewport, so move us to the end of
  // the document (trick from StackOverflow), then back to the text cursor,
  // then center on the cursor.
  if (auto next_block = text_cursor.block().blockNumber();
      next_block != d->last_block) {
    d->text_edit->moveCursor(QTextCursor::End);
    d->text_edit->setTextCursor(text_cursor);
    d->text_edit->ensureCursorVisible();
    d->text_edit->centerCursor();
    d->last_block = next_block;

  // The line on which we last clicked is likely visible. Don't center us on
  // the target cursor.
  } else {
    d->text_edit->setTextCursor(text_cursor);
  }

  if (opt_end.has_value()) {
    text_cursor.setPosition(opt_end.value(), QTextCursor::MoveMode::KeepAnchor);
    d->text_edit->setTextCursor(text_cursor);
  }

  ResumeCursorTracking();
  return true;
}

QString CodeView::Text() const {
  return d->text_edit->toPlainText();
}

void CodeView::SetWordWrapping(bool enabled) {
  d->text_edit->setWordWrapMode(enabled ? QTextOption::WordWrap
                                        : QTextOption::NoWrap);
}

bool CodeView::ScrollToLineNumber(unsigned line) {
  d->deferred_scroll_to_line.reset();

  if (!d->model->IsReady()) {
    d->deferred_scroll_to_line = line;
    return false;
  }

  auto opt_block_number = GetBlockNumberFromLineNumber(d->token_map, line);
  if (!opt_block_number.has_value()) {
    return false;
  }

  const auto &block_number = opt_block_number.value();

  const auto &document = *d->text_edit->document();
  auto text_block = document.findBlockByNumber(static_cast<int>(block_number));
  if (!text_block.isValid()) {
    return false;
  }

  return SetCursorPosition(text_block.position(), std::nullopt);
}

CodeView::CodeView(ICodeModel *model, QWidget *parent)
    : ICodeView(parent),
      d(new PrivateData) {

  InstallModel(model);
  InitializeWidgets();

  SetWordWrapping(false);
}

bool CodeView::eventFilter(QObject *obj, QEvent *event) {
  //if (dynamic_cast<QMouseEvent *>(event) && obj == d->text_edit->viewport()) {
  //  qDebug() << "eventFilter" << event->type()
  //           << d->text_edit->textCursor().position();
  //}

  switch (event->type()) {
    case QEvent::Paint:
      if (obj == d->gutter) {
        OnGutterPaintEvent(dynamic_cast<QPaintEvent *>(event));
        return true;
      }
      break;
    case QEvent::MouseMove:
      if (obj == d->text_edit->viewport()) {
        OnTextEditViewportMouseMoveEvent(dynamic_cast<QMouseEvent *>(event));
      }
      break;
    case QEvent::MouseButtonPress:
      if (obj == d->text_edit->viewport()) {
        auto mouse_event = dynamic_cast<QMouseEvent *>(event);
        QTextCursor cursor = d->text_edit->textCursor();
        d->last_press_version = d->version;
        d->last_press_position = cursor.position();
        d->last_block = cursor.block().blockNumber();
        OnTextEditViewportMouseButtonPress(mouse_event);
      }
      break;
    case QEvent::MouseButtonRelease:
      if (obj == d->text_edit->viewport()) {
        auto prev_version = d->last_press_version;
        auto prev_position = d->last_press_position;
        d->last_press_version = -1;
        d->last_press_position = -1;

        // If between the last press and now the "cursor version" changed, i.e.
        // external code forced us to scroll to a different line, or the model
        // was reset, then ignore the mouse release.
        if (prev_version != d->version) {
          event->ignore();

          // Sometimes a model reset as a reuslt of a mouse press triggers a
          // selection of everything from the beginning of the text to where
          // the cursor is upon mouse press release (usually following a minor
          // mouse move). If we observe a selection at this point, and if the
          // beginning or ending of the selection matches our pre-model reset
          // position, then the position must still be valid, and so we'll move
          // the cursor back to where it was.
          QTextCursor cursor = d->text_edit->textCursor();
          if (cursor.selectionStart() != cursor.selectionEnd() &&
              (prev_position == cursor.selectionStart() ||
               prev_position == cursor.selectionEnd())) {

            SetCursorPosition(prev_position, std::nullopt);
          }
          return true;
        }
      }
      break;
    case QEvent::KeyPress:
      if (obj == d->text_edit) {
        auto key_event = dynamic_cast<QKeyEvent *>(event);
        OnTextEditViewportKeyboardButtonPress(key_event);
      }
      break;
    case QEvent::Wheel:
      if (auto wheel_event = dynamic_cast<QWheelEvent *>(event);
          obj == d->text_edit->viewport() &&
          (wheel_event->modifiers() & Qt::ControlModifier) != 0) {
        d->last_block = -1;
        OnTextEditTextZoom(wheel_event);
      }
      break;
    default:
      break;
  }
  return false;
}

void CodeView::InstallModel(ICodeModel *model) {
  d->model = model;
  d->model->setParent(this);

  connect(d->model, &ICodeModel::ModelReset, this, &CodeView::OnModelReset);

  connect(d->model, &ICodeModel::ModelResetSkipped, this,
          &CodeView::OnModelResetDone);
}

void CodeView::InitializeWidgets() {
  // Code viewer
  d->text_edit = new QPlainTextEditMod();
  d->text_edit->setReadOnly(true);
  d->text_edit->setContextMenuPolicy(Qt::NoContextMenu);
  d->text_edit->setTextInteractionFlags(Qt::TextSelectableByMouse |
                                        Qt::TextSelectableByKeyboard);
  d->text_edit->installEventFilter(this);
  d->text_edit->viewport()->installEventFilter(this);
  d->text_edit->viewport()->setMouseTracking(true);

  connect(d->text_edit, &QPlainTextEditMod::updateRequest,
          this, &CodeView::OnTextEditUpdateRequest);

  // Gutter
  d->gutter = new QWidget();
  d->gutter->installEventFilter(this);

  // Search widget
  d->search_widget = ISearchWidget::Create(ISearchWidget::Mode::Search, this);
  connect(d->search_widget, &ISearchWidget::SearchParametersChanged,
          this, &CodeView::OnSearchParametersChange);

  connect(d->search_widget, &ISearchWidget::ShowSearchResult,
          this, &CodeView::OnShowSearchResult);

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

  // Apply the default tab stop distance
  SetTabWidth(d->tab_width);

  // Initialize the go-to-line widget
  // TODO: Is there a default binding we can use which is already
  // defined in Qt?
  d->go_to_line_widget = new GoToLineWidget(this);
  d->go_to_line_shortcut = new QShortcut(
      QKeySequence(Qt::CTRL | Qt::Key_L), this, this,
      &CodeView::OnGoToLineTriggered, Qt::WidgetWithChildrenShortcut);

  connect(d->go_to_line_widget, &GoToLineWidget::LineNumberChanged, this,
          &CodeView::OnGoToLine);

  // This will also cause a model reset update
  static const auto kRequestDarkTheme{true};
  SetTheme(GetDefaultCodeViewTheme(kRequestDarkTheme));

  // NOTE(pag): This has to go last, as it requires all things to be
  //            initialized.
  ConnectCursorChangeEvent();
}

//! Disable cursor change tracking.
void CodeView::StopCursorTracking(void) {
  if (disconnect(d->cursor_change_signal)) {
    QMetaObject::Connection().swap(d->cursor_change_signal);
  }
}

//! Re-introduce cursor change tracking.
void CodeView::ResumeCursorTracking(void) {
  QTimer::singleShot(200, this, &CodeView::ConnectCursorChangeEvent);
}

//! Connect the cursor changed event. This will also trigger a cursor event.
void CodeView::ConnectCursorChangeEvent(void) {
  d->cursor_change_signal =
      connect(d->text_edit, &QPlainTextEditMod::cursorPositionChanged,
              this, &CodeView::OnCursorMoved);

  OnCursorMoved();
}

std::optional<CodeModelIndex>
CodeView::GetCodeModelIndexFromMousePosition(const QPoint &pos) {
  QTextCursor text_cursor = d->text_edit->cursorForPosition(pos);
  return GetCodeModelIndexFromTextCursor(d->token_map, text_cursor);
}

void CodeView::OnTextEditViewportMouseMoveEvent(QMouseEvent *event) {
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
  emit TokenTriggered({TokenAction::Type::Hover, std::nullopt}, model_index);
}

void CodeView::OnTextEditViewportMouseButtonPress(QMouseEvent *event) {
  auto opt_model_index = GetCodeModelIndexFromMousePosition(event->pos());
  if (!opt_model_index.has_value()) {
    return;
  }

  const CodeModelIndex &model_index = opt_model_index.value();

  if (event->button() == Qt::LeftButton &&
      event->modifiers() == Qt::ControlModifier) {
    emit TokenTriggered({TokenAction::Type::Primary, std::nullopt},
                        model_index);

  } else if (event->button() == Qt::RightButton &&
             event->modifiers() == Qt::NoModifier) {
    emit TokenTriggered({TokenAction::Type::Secondary, std::nullopt},
                        model_index);
  }
}

void CodeView::OnTextEditViewportKeyboardButtonPress(QKeyEvent *event) {
  auto text_cursor = d->text_edit->textCursor();

  auto opt_model_index =
      GetCodeModelIndexFromTextCursor(d->token_map, text_cursor);

  if (!opt_model_index.has_value()) {
    return;
  }

  TokenAction::KeyboardButton keyboard_button;
  keyboard_button.key = event->key();

  keyboard_button.shift_modifier =
      (event->modifiers() & Qt::ShiftModifier) != 0;

  keyboard_button.control_modifier =
      (event->modifiers() & Qt::ControlModifier) != 0;

  const CodeModelIndex &model_index = opt_model_index.value();
  emit TokenTriggered({TokenAction::Type::Keyboard, keyboard_button},
                      model_index);
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
  return (static_cast<std::uint64_t>(index.row) << 32u) |
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

std::optional<std::size_t>
CodeView::GetBlockNumberFromLineNumber(const TokenMap &token_map,
                                       unsigned line_number) {

  auto block_number_it =
      token_map.line_number_to_block_number.find(line_number);

  if (block_number_it == token_map.line_number_to_block_number.end()) {
    return std::nullopt;
  }

  return block_number_it->second;
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

    if (cursor_position >= token_map_entry.cursor_start &&
        cursor_position <= token_map_entry.cursor_end) {

      return token_map_entry.model_index;
    }
  }

  return std::nullopt;
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

  Count row_count = model.RowCount();
  if (!row_count) {
    return document;
  }

  QTextCursor cursor(document);
  cursor.beginEditBlock();

  auto L_updateProgress = [&](std::uint64_t current_row) -> bool {
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

    auto current_progress = (current_row * 100u) / row_count;
    return progress_callback(static_cast<int>(current_progress));
  };

  for (Count row_index = 0u; row_index < row_count; ++row_index) {
    if (!L_updateProgress(row_index)) {
      break;
    }

    CodeModelIndex model_index = {&model, row_index, 0};
    auto line_mappings_need_update{true};
    Count token_count = model.TokenCount(row_index);

    for (Count token_index = 0; token_index < token_count;
         ++token_index) {
      // Update the highest line number value
      const auto &line_number_var =
          model.Data(model_index, ICodeModel::LineNumberRole);

      if (line_number_var.isValid()) {
        auto line_number = qvariant_cast<Count>(line_number_var);

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

      // Add the entry to the block number index.
      {
        auto block_number = cursor.blockNumber();
        auto &unique_token_id_list =
            token_map.block_number_to_unique_token_id_list[block_number];
        unique_token_id_list.push_back(unique_token_id);
      }

      // Add the entry to the token group index.
      const auto &token_group_id_var =
          model.Data(model_index, ICodeModel::TokenGroupIdRole);

      if (token_group_id_var.isValid()) {
        auto token_group_id = qvariant_cast<std::uint64_t>(token_group_id_var);
        auto &unique_token_id_list =
            token_map.token_group_id_to_unique_token_id_list[token_group_id];
        unique_token_id_list.push_back(unique_token_id);
      }

      // Add the entry to the related entity id index. We use this to highlight
      // other tokens sharing the same related entity id.
      const auto &related_entity_id_var =
          model.Data(model_index, ICodeModel::RealRelatedEntityIdRole);

      if (related_entity_id_var.isValid()) {
        auto related_entity_id =
            qvariant_cast<RawEntityId>(related_entity_id_var);

        auto &unique_token_id_list =
            token_map.related_entity_id_to_unique_token_id_list[related_entity_id];

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

void CodeView::ConfigureTextFormatFromTheme(
    QTextCharFormat &text_format, const CodeViewTheme &theme,
    const QVariant &token_category_var) {
  if (!token_category_var.isValid()) {
    return;
  }

  bool is_ok = true;
  auto token_category_uint = token_category_var.toUInt(&is_ok);
  if (!is_ok || token_category_uint >= NumEnumerators(TokenCategory{})) {
    return;
  }

  auto token_category = static_cast<TokenCategory>(token_category_uint);
  text_format.setBackground(theme.BackgroundColor(token_category));
  text_format.setForeground(theme.ForegroundColor(token_category));
  CodeViewTheme::Style text_style = theme.TextStyle(token_category);
  text_format.setFontItalic(text_style.italic);
  text_format.setFontWeight(text_style.bold ? QFont::DemiBold : QFont::Normal);
  text_format.setFontUnderline(text_style.underline);
  text_format.setFontStrikeOut(text_style.strikeout);
}

QList<QTextEdit::ExtraSelection> CodeView::GenerateExtraSelections(
    const TokenMap &token_map, QPlainTextEdit &text_edit,
    const ICodeModel &model, const CodeViewTheme &theme) {

  QList<QTextEdit::ExtraSelection> extra_selection_list;

  QTextEdit::ExtraSelection selection;

  std::unordered_set<std::uint64_t> visited_model_index_list;
  std::unordered_set<Count> colored_line_list;

  for (const Count &colored_line : colored_line_list) {
    auto column_count = model.TokenCount(colored_line);

    for (Count column = 0; column < column_count; ++column) {
      auto unique_token_id = GetUniqueTokenIdentifier(
          {&model, colored_line, column});

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

void CodeView::HighlightTokensForRelatedEntityID(
    const TokenMap &token_map, const QTextCursor &text_cursor,
    RawEntityId related_entity_id,
    QList<QTextEdit::ExtraSelection> &selection_list,
    const CodeViewTheme &theme) {

  auto unique_token_id_list =
      token_map.related_entity_id_to_unique_token_id_list.at(related_entity_id);

  QTextEdit::ExtraSelection selection;
  for (auto unique_token_id : unique_token_id_list) {
    const auto &token_map_entry = token_map.data.at(unique_token_id);

    selection = {};
    selection.format.setBackground(theme.highlighted_entity_background_color);

    selection.cursor = text_cursor;

    selection.cursor.setPosition(token_map_entry.cursor_start,
                                 QTextCursor::MoveMode::MoveAnchor);
    selection.cursor.setPosition(token_map_entry.cursor_end,
                                 QTextCursor::MoveMode::KeepAnchor);

    selection_list.append(std::move(selection));
  }
}

void CodeView::OnModelReset() {
  d->version++;
  d->last_block = -1;
  d->search_widget->Deactivate();
  d->go_to_line_widget->Deactivate();
  d->opt_prev_hovered_model_index.reset();

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
  OnModelResetDone();
}

void CodeView::OnModelResetDone() {

  // If there was a request to scroll to a line, but the document wasn't ready
  // at the time of the request, then enact the scroll now.
  if (d->deferred_scroll_to_line.has_value()) {
    ScrollToLineNumber(d->deferred_scroll_to_line.value());
  }

  emit DocumentChanged();
}

void CodeView::OnGutterPaintEvent(QPaintEvent *event) {
  QPainter painter(d->gutter);
  painter.fillRect(event->rect(), d->theme.default_gutter_background);

  painter.setPen(d->theme.ForegroundColor(TokenCategory::LINE_NUMBER));
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

//! Called when the cursor position has changed.
void CodeView::OnCursorMoved(void) {
  if (!d->model->IsReady()) {
    return;
  }

  QList<QTextEdit::ExtraSelection> extra_selections;
  QTextEdit::ExtraSelection selection;

  // Highlight the current line where the cursor is.
  selection.format.setBackground(d->theme.selected_line_background_color);
  selection.format.setProperty(QTextFormat::FullWidthSelection, true);
  selection.cursor = d->text_edit->textCursor();
  selection.cursor.clearSelection();
  extra_selections.append(selection);

  auto opt_model_index =
      GetCodeModelIndexFromTextCursor(d->token_map, d->text_edit->textCursor());

  // Try to highlight all entities related to the entity on which the cursor
  // is hovering.
  //
  // NOTE(pag): We use `RealRelatedEntityIdRole` instead of the usual
  //            `RelatedEntityIdRole` because the code preview alters
  //            the related entity ID to be the token ID via a proxy model.
  if (opt_model_index.has_value()) {
    auto unique_token_id = GetUniqueTokenIdentifier(opt_model_index.value());
    const auto &token_map_entry = d->token_map.data.at(unique_token_id);
    QVariant related_entity_id_var = d->model->Data(
        token_map_entry.model_index, ICodeModel::RealRelatedEntityIdRole);

    if (related_entity_id_var.isValid()) {
      auto related_entity_id =
          qvariant_cast<std::uint64_t>(related_entity_id_var);
      HighlightTokensForRelatedEntityID(
          d->token_map, d->text_edit->textCursor(), related_entity_id,
          extra_selections, d->theme);
    }
  }

  d->text_edit->setExtraSelections(extra_selections);

  // Tell users of the code view when the cursor moves.
  if (opt_model_index.has_value()) {
    emit CursorMoved(opt_model_index.value());
  }
}

void CodeView::OnSearchParametersChange(
    const ISearchWidget::SearchParameters &search_parameters) {

  d->search_result_list.clear();
  if (search_parameters.pattern.empty()) {
    return;
  }

  QRegularExpression::PatternOptions options{
      QRegularExpression::NoPatternOption};

  QTextDocument::FindFlags find_flags{};

  if (!search_parameters.case_sensitive) {
    options |= QRegularExpression::CaseInsensitiveOption;
  } else {
    find_flags = QTextDocument::FindCaseSensitively;
  }

  auto pattern = QString::fromStdString(search_parameters.pattern);
  if (search_parameters.type == ISearchWidget::SearchParameters::Type::Text) {
    pattern = QRegularExpression::escape(pattern);
  }

  if (search_parameters.whole_word) {
    find_flags |= QTextDocument::FindWholeWords;

    if (search_parameters.type == ISearchWidget::SearchParameters::Type::Text) {
      pattern = "\\b" + pattern + "\\b";
    }
  }

  QRegularExpression regex(pattern, options);

  // The regex is already validated by the search widget
  Assert(regex.isValid(),
         "Invalid regex found in CodeView::OnSearchParametersChange");

  const auto &document = *d->text_edit->document();

  int current_position{};

  for (;;) {
    auto text_cursor = document.find(regex, current_position, find_flags);
    if (text_cursor.isNull()) {
      break;
    }

    current_position = text_cursor.selectionEnd();

    auto result = std::make_pair(text_cursor.selectionStart(),
                                 text_cursor.selectionEnd());
    d->search_result_list.push_back(result);
  }

  d->search_widget->UpdateSearchResultCount(d->search_result_list.size());
}

void CodeView::OnShowSearchResult(const std::size_t &result_index) {
  if (result_index >= d->search_result_list.size()) {
    return;
  }

  const auto &search_result = d->search_result_list.at(result_index);
  SetCursorPosition(search_result.first, search_result.second);
}

void CodeView::OnGoToLineTriggered() {
  d->go_to_line_widget->Activate(d->token_map.highest_line_number);
}

void CodeView::OnGoToLine(unsigned line_number) {
  ScrollToLineNumber(line_number);
}

}  // namespace mx::gui
