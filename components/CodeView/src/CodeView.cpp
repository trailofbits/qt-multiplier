/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "CodeView.h"
#include "GoToLineWidget.h"

#include <multiplier/GUI/Assert.h>

#include <multiplier/Frontend/TokenCategory.h>

#include <QAbstractProxyModel>
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
#include <QScrollBar>

#include <unordered_set>
#include <vector>
#include <iostream>

namespace mx::gui {
namespace {

const int kHoverMsecsTimer{2000};

class QPlainTextEditMod final : public QPlainTextEdit {
 public:
  QPlainTextEditMod(QWidget *parent = nullptr) : QPlainTextEdit(parent) {}

  virtual ~QPlainTextEditMod() override{};

  friend class mx::gui::CodeView;
};

}  // namespace

struct CodeView::PrivateData final {
  QAbstractItemModel *model{nullptr};

  int version{0};
  int last_press_version{-1};
  int last_press_position{-1};
  int last_block{-1};

  bool browser_mode{true};
  QPlainTextEditMod *text_edit{nullptr};
  QWidget *gutter{nullptr};

  QMetaObject::Connection cursor_change_signal;

  ISearchWidget *search_widget{nullptr};
  std::vector<std::pair<int, int>> search_result_list;

  TokenMap token_map;

  CodeViewTheme theme;
  std::size_t tab_width{4};

  std::optional<QModelIndex> opt_prev_hovered_model_index;
  QTimer hover_timer;

  QShortcut *go_to_line_shortcut{nullptr};
  GoToLineWidget *go_to_line_widget{nullptr};

  qreal default_font_point_size{};
  QShortcut *zoom_in_shortcut{nullptr};
  QShortcut *zoom_out_shortcut{nullptr};
  QShortcut *reset_zoom_shortcut{nullptr};

  std::optional<unsigned> deferred_scroll_to_line;
  QList<QTextEdit::ExtraSelection> extra_selection_list;

  QTimer update_timer;
};

void CodeView::SetTheme(const CodeViewTheme &theme) {
  d->theme = theme;

  QFont font(d->theme.font_name);
  font.setStyleHint(QFont::TypeWriter);
  setFont(font);

  d->default_font_point_size = font.pointSizeF();

  auto palette = d->text_edit->palette();
  palette.setColor(QPalette::Window, d->theme.default_background_color);
  palette.setColor(QPalette::WindowText, d->theme.default_foreground_color);

  palette.setColor(QPalette::Base, d->theme.default_background_color);
  palette.setColor(QPalette::Text, d->theme.default_foreground_color);

  palette.setColor(QPalette::AlternateBase, d->theme.default_background_color);
  d->text_edit->setPalette(palette);

  const auto L_saveScrollBarValue =
      [](const QScrollBar *scrollbar) -> std::optional<int> {
    if (scrollbar == nullptr || !scrollbar->isEnabled()) {
      return std::nullopt;
    }

    return scrollbar->value();
  };

  const auto L_restoreScrollBarValue = [](QScrollBar *scrollbar,
                                          const std::optional<int> &opt_value) {
    if (scrollbar == nullptr || !scrollbar->isEnabled() ||
        !opt_value.has_value()) {
      return;
    }

    scrollbar->setValue(opt_value.value());
  };

  auto opt_vertical_scroll =
      L_saveScrollBarValue(d->text_edit->verticalScrollBar());
  auto opt_horizontal_scroll =
      L_saveScrollBarValue(d->text_edit->horizontalScrollBar());

  OnModelReset();

  L_restoreScrollBarValue(d->text_edit->verticalScrollBar(),
                          opt_vertical_scroll);
  L_restoreScrollBarValue(d->text_edit->horizontalScrollBar(),
                          opt_horizontal_scroll);
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
  QTextCursor text_cursor = d->text_edit->textCursor();
  auto prev_position = text_cursor.position();

  // NOTE(pag): We stop cursor tracking so that the individual cursor
  //            manipulations here that are needed to center the view on the
  //            cursor don't bubble up to higher levels.
  StopCursorTracking();

  text_cursor.movePosition(QTextCursor::End);
  auto max_position = text_cursor.position();

  if (start >= max_position || opt_end.value_or(start) >= max_position) {
    text_cursor.setPosition(prev_position, QTextCursor::MoveAnchor);
    ResumeCursorTracking();
    return false;
  }

  text_cursor.setPosition(start, QTextCursor::MoveMode::MoveAnchor);

  // We want to change the scroll in the viewport, so move us to the end of
  // the document (trick from StackOverflow), then back to the text cursor,
  // then center on the cursor.
  auto next_block = text_cursor.block().blockNumber();
  if (next_block != d->last_block) {
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

  // Update these to pretend a press and suppress a mouse release.
  d->last_press_position = text_cursor.position();
  d->last_block = next_block;

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
  auto model_state_var =
      d->model->data(QModelIndex(), ICodeModel::ModelStateRole);

  Assert(model_state_var.isValid(), "This should always work");
  ICodeModel::ModelState state =
      static_cast<ICodeModel::ModelState>(model_state_var.toInt());

  if (state == ICodeModel::ModelState::Ready) {
    d->deferred_scroll_to_line.reset();
    return ScrollToLineNumberInternal(line);
  }

  d->deferred_scroll_to_line = line;
  return false;
}

void CodeView::SetBrowserMode(const bool &enabled) {
  d->browser_mode = enabled;
}

CodeView::CodeView(QAbstractItemModel *model, QWidget *parent)
    : ICodeView(parent),
      d(new PrivateData) {

  InstallModel(model);
  InitializeWidgets();

  SetWordWrapping(false);
  OnModelReset();

  connect(&ThemeManager::Get(), &ThemeManager::ThemeChanged, this,
          &CodeView::OnThemeChange);

  connect(&d->update_timer, &QTimer::timeout, this,
          &CodeView::DocumentChangedTimedSignal);
}

bool CodeView::eventFilter(QObject *obj, QEvent *event) {
  //  if (dynamic_cast<QMouseEvent *>(event) && obj == d->text_edit->viewport()) {
  //    qDebug() << "eventFilter" << event->type()
  //             << "position" << d->text_edit->textCursor().position()
  //             << "version" << d->version
  //             << "last_press_version" << d->last_press_version
  //             << "last_press_position" << d->last_press_position;
  //  }

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
    case QEvent::MouseButtonDblClick: return true;
    case QEvent::MouseButtonPress:
      if (obj == d->text_edit->viewport()) {
        auto mouse_event = dynamic_cast<QMouseEvent *>(event);
        return OnTextEditViewportMouseButtonPress(mouse_event);
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

          // Sometimes a model reset as a result of a mouse press triggers a
          // selection of everything from the beginning of the text to where
          // the cursor is upon mouse press release (usually following a minor
          // mouse move). If we observe a selection at this point, and if the
          // beginning or ending of the selection matches our pre-model reset
          // position, then the position must still be valid, and so we'll move
          // the cursor back to where it was.
          if (prev_position != -1 && d->last_block != -1) {
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
    default: break;
  }
  return false;
}

namespace {

static ICodeModel *UnderlyingCodeModel(QAbstractItemModel *model) {
  if (auto code_model = dynamic_cast<ICodeModel *>(model)) {
    return code_model;
  }

  if (auto proxy = dynamic_cast<QAbstractProxyModel *>(model)) {
    return UnderlyingCodeModel(proxy->sourceModel());
  }

  return nullptr;
}

}  // namespace

void CodeView::InstallModel(QAbstractItemModel *model) {
  d->model = model;
  d->model->setParent(this);

  connect(d->model, &QAbstractItemModel::modelReset, this,
          &CodeView::OnModelReset);

  connect(d->model, &QAbstractItemModel::rowsInserted, this,
          &CodeView::OnRowsInserted);

  connect(d->model, &QAbstractItemModel::rowsRemoved, this,
          &CodeView::OnRowsRemoved);

  connect(d->model, &QAbstractItemModel::dataChanged, this,
          &CodeView::OnDataChange);

  if (auto code_model = UnderlyingCodeModel(d->model)) {
    connect(code_model, &ICodeModel::EntityLocation, this,
            &CodeView::OnEntityLocation);
  }
}

void CodeView::InitializeWidgets() {
  // Initialize the hover timer
  d->hover_timer.setSingleShot(true);
  connect(&d->hover_timer, &QTimer::timeout, this,
          &CodeView::OnHoverTimerTimeout);

  // Code viewer
  d->text_edit = new QPlainTextEditMod();
  d->text_edit->setReadOnly(true);
  d->text_edit->setAcceptDrops(false);
  d->text_edit->setContextMenuPolicy(Qt::NoContextMenu);
  d->text_edit->setTextInteractionFlags(Qt::TextSelectableByMouse |
                                        Qt::TextSelectableByKeyboard);
  d->text_edit->installEventFilter(this);
  d->text_edit->setMouseTracking(true);
  d->text_edit->viewport()->installEventFilter(this);
  d->text_edit->viewport()->setMouseTracking(true);

  connect(d->text_edit, &QPlainTextEditMod::updateRequest, this,
          &CodeView::OnTextEditUpdateRequest);

  // Gutter
  d->gutter = new QWidget();
  d->gutter->installEventFilter(this);

  // Search widget
  d->search_widget = ISearchWidget::Create(ISearchWidget::Mode::Search, this);
  connect(d->search_widget, &ISearchWidget::SearchParametersChanged, this,
          &CodeView::OnSearchParametersChange);

  connect(d->search_widget, &ISearchWidget::ShowSearchResult, this,
          &CodeView::OnShowSearchResult);

  // Layout for the gutter and code view
  auto code_layout = new QHBoxLayout(this);
  code_layout->setContentsMargins(0, 0, 0, 0);
  code_layout->setSpacing(0);
  code_layout->addWidget(d->gutter);
  code_layout->addWidget(d->text_edit);

  // Main layout
  auto main_layout = new QVBoxLayout(this);
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

  // Initialize the zoom shortcuts. The reset zoom does not have a standard
  // keybinding. Most programs use CTRL+Numpad0 but it is not uncommon
  // to lack a numpad nowdays. Rebind it to CTRL+0 like browsers do.
  d->zoom_in_shortcut =
      new QShortcut(QKeySequence::ZoomIn, this, this, &CodeView::OnZoomIn,
                    Qt::WidgetWithChildrenShortcut);

  d->zoom_out_shortcut =
      new QShortcut(QKeySequence::ZoomOut, this, this, &CodeView::OnZoomOut,
                    Qt::WidgetWithChildrenShortcut);

  d->reset_zoom_shortcut =
      new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_0), this, this,
                    &CodeView::OnResetZoom, Qt::WidgetWithChildrenShortcut);

  // This will also cause a model reset update
  const auto &theme = ThemeManager::Get().GetCodeViewTheme();
  SetTheme(theme);

  // NOTE(pag): This has to go last, as it requires all things to be
  //            initialized.
  ConnectCursorChangeEvent();
}

void CodeView::StopCursorTracking(void) {
  if (disconnect(d->cursor_change_signal)) {
    QMetaObject::Connection().swap(d->cursor_change_signal);
  }
}

void CodeView::ResumeCursorTracking(void) {
  d->version++;
  QTimer::singleShot(200, this, &CodeView::ConnectCursorChangeEvent);
}

void CodeView::ConnectCursorChangeEvent(void) {
  d->cursor_change_signal =
      connect(d->text_edit, &QPlainTextEditMod::cursorPositionChanged, this,
              &CodeView::OnCursorMoved);

  OnCursorMoved();
}

std::optional<QModelIndex>
CodeView::GetModelIndexFromMousePosition(const QPoint &pos) {
  QTextCursor text_cursor = d->text_edit->cursorForPosition(pos);
  return GetQModelIndexFromTextCursor(*d->model, d->token_map, text_cursor);
}

void CodeView::OnTextEditViewportMouseMoveEvent(QMouseEvent *event) {
  const auto L_updateMouseCursor = [&](const bool &interactive) {
    if (interactive) {
      d->text_edit->viewport()->setCursor(Qt::PointingHandCursor);

    } else {
      d->text_edit->viewport()->setCursor(d->browser_mode ? Qt::ArrowCursor
                                                          : Qt::IBeamCursor);
    }
  };

  auto opt_model_index = GetModelIndexFromMousePosition(event->pos());
  if (!opt_model_index.has_value()) {
    d->opt_prev_hovered_model_index = std::nullopt;

    L_updateMouseCursor(false);
    return;
  }

  const auto &model_index = opt_model_index.value();

  auto token_id_var = model_index.data(ICodeModel::TokenIdRole);
  if (!token_id_var.isValid()) {
    L_updateMouseCursor(false);
    return;
  }

  auto token_id = qvariant_cast<RawEntityId>(token_id_var);

  auto related_token_id_var = model_index.data(ICodeModel::RelatedEntityIdRole);
  auto has_related_entity_id{related_token_id_var.isValid() &&
                             qvariant_cast<RawEntityId>(related_token_id_var) !=
                                 kInvalidEntityId};

  auto is_interactive =
      has_related_entity_id &&
      (d->browser_mode || event->modifiers() == Qt::ControlModifier);

  L_updateMouseCursor(is_interactive);

  if (d->opt_prev_hovered_model_index.has_value()) {
    const auto &prev_hovered_model_index =
        d->opt_prev_hovered_model_index.value();

    auto prev_token_id_var =
        prev_hovered_model_index.data(ICodeModel::TokenIdRole);

    auto prev_token_id = qvariant_cast<RawEntityId>(prev_token_id_var);
    if (prev_token_id == token_id) {
      return;
    }
  }

  d->opt_prev_hovered_model_index = model_index;
  d->hover_timer.start(kHoverMsecsTimer);
}

void CodeView::OnHoverTimerTimeout() {
  if (!d->opt_prev_hovered_model_index.has_value()) {
    return;
  }

  auto prev_hovered_model_index = d->opt_prev_hovered_model_index.value();
  d->opt_prev_hovered_model_index = std::nullopt;

  auto cursor_pos = d->text_edit->viewport()->mapFromGlobal(QCursor::pos());
  auto opt_model_index = GetModelIndexFromMousePosition(cursor_pos);
  if (!opt_model_index.has_value()) {
    return;
  }

  const auto &model_index = opt_model_index.value();
  if (!model_index.isValid()) {
    return;
  }

  if (!prev_hovered_model_index.isValid()) {
    return;
  }

  auto token_id_var = model_index.data(ICodeModel::TokenIdRole);
  auto token_id = qvariant_cast<RawEntityId>(token_id_var);

  auto prev_token_id_var =
      prev_hovered_model_index.data(ICodeModel::TokenIdRole);

  auto prev_token_id = qvariant_cast<RawEntityId>(prev_token_id_var);
  if (token_id != prev_token_id) {
    return;
  }

  emit TokenTriggered({TokenAction::Type::Hover, std::nullopt}, model_index);
}

bool CodeView::OnTextEditViewportMouseButtonPress(QMouseEvent *event) {
  QTextCursor cursor = d->text_edit->cursorForPosition(event->pos());
  auto opt_model_index =
      GetQModelIndexFromTextCursor(*d->model, d->token_map, cursor);

  if (event->button() == Qt::LeftButton) {
    enum class InteractionType {
      None,
      TextCursor,
      TokenAction,
    };

    auto interaction_type{InteractionType::None};
    if (d->browser_mode) {
      if (event->modifiers() == Qt::ControlModifier) {
        interaction_type = InteractionType::TextCursor;

      } else if (event->modifiers() == Qt::NoModifier) {
        interaction_type = InteractionType::TokenAction;
      }

    } else {
      if (event->modifiers() == Qt::ControlModifier) {
        interaction_type = InteractionType::TokenAction;

      } else if (event->modifiers() == Qt::NoModifier) {
        interaction_type = InteractionType::TextCursor;
      }
    }

    if (interaction_type == InteractionType::None) {
      return true;
    }

    switch (interaction_type) {
      case InteractionType::None: return true;

      case InteractionType::TextCursor: {
        d->last_press_version = d->version;
        d->last_press_position = cursor.position();
        d->last_block = cursor.block().blockNumber();

        HandleNewCursor(cursor);
        return false;
      }

      case InteractionType::TokenAction: {
        if (opt_model_index.has_value()) {
          auto model_index = opt_model_index.value();
          emit TokenTriggered({TokenAction::Type::Primary, std::nullopt},
                              model_index);
        }

        return true;
      }
    }

  } else if (event->button() == Qt::RightButton &&
             event->modifiers() == Qt::NoModifier) {

    if (!opt_model_index.has_value()) {
      return true;
    }

    auto model_index = opt_model_index.value();
    emit TokenTriggered({TokenAction::Type::Secondary, std::nullopt},
                        model_index);

    return true;

  } else {
    return true;
  }
}

void CodeView::OnTextEditViewportKeyboardButtonPress(QKeyEvent *event) {
  auto text_cursor = d->text_edit->textCursor();

  auto opt_model_index =
      GetQModelIndexFromTextCursor(*d->model, d->token_map, text_cursor);

  if (!opt_model_index.has_value()) {
    return;
  }

  TokenAction::KeyboardButton keyboard_button;
  keyboard_button.key = event->key();
  if (keyboard_button.key == Qt::Key_Shift ||
      keyboard_button.key == Qt::Key_Control) {
    return;
  }

  keyboard_button.shift_modifier =
      (event->modifiers() & Qt::ShiftModifier) != 0;

  keyboard_button.control_modifier =
      (event->modifiers() & Qt::ControlModifier) != 0;

  const QModelIndex &model_index = opt_model_index.value();

  // Keep some debug data for now. If the token is different than the
  // one that was shown on screen, then there is a mismatch between
  // the TokenMap::block_entry_list entries and the test blocks in the
  // QTextDocument.
  std::cerr << __FILE__ << "@" << __LINE__ << " Keyboard action on '"
            << model_index.data().toString().toStdString() << "'\n";

  emit TokenTriggered({TokenAction::Type::Keyboard, keyboard_button},
                      model_index);
}

void CodeView::OnTextEditTextZoom(QWheelEvent *event) {
  qreal delta = static_cast<qreal>(event->angleDelta().y()) / 120.0;
  if (delta == 0.0) {
    return;
  }

  SetZoomDelta(delta);
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

void CodeView::UpdateBaseExtraSelections() {
  QList<QTextEdit::ExtraSelection> extra_selection_list;

  auto row_count = d->model->rowCount();
  const auto &document = *d->text_edit->document();

  for (int row{}; row < row_count; ++row) {
    auto line_index = d->model->index(row, 0);
    if (!line_index.isValid()) {
      break;
    }

    auto column_count = d->model->columnCount(line_index);

    for (int column{}; column < column_count; ++column) {
      auto token_index = d->model->index(0, column, line_index);
      if (!token_index.isValid()) {
        break;
      }

      auto background_color_var = token_index.data(Qt::BackgroundRole);
      if (!background_color_var.isValid()) {
        continue;
      }

      const auto &background_color =
          qvariant_cast<QColor>(background_color_var);

      auto foreground_color_var = token_index.data(Qt::ForegroundRole);
      auto foreground_color = qvariant_cast<QColor>(foreground_color_var);

      QTextEdit::ExtraSelection selection = {};
      selection.format.setBackground(background_color);
      selection.format.setForeground(foreground_color);

      const auto &block_entry =
          d->token_map.block_entry_list[static_cast<std::size_t>(row)];

      const auto &token_entry =
          block_entry.token_entry_list[static_cast<std::size_t>(column)];

      auto text_block = document.findBlockByNumber(row);

      selection.cursor = QTextCursor(text_block);
      selection.cursor.setPosition(
          text_block.position() + token_entry.cursor_start,
          QTextCursor::MoveMode::MoveAnchor);

      selection.cursor.setPosition(
          text_block.position() + token_entry.cursor_end,
          QTextCursor::MoveMode::KeepAnchor);

      extra_selection_list.append(std::move(selection));
    }
  }

  d->extra_selection_list = std::move(extra_selection_list);
  d->text_edit->setExtraSelections(d->extra_selection_list);
}

std::uint64_t CodeView::GetUniqueTokenIdentifier(const QModelIndex &index) {
  auto node_id = static_cast<std::uint64_t>(index.internalId());
  auto index_column = static_cast<std::uint64_t>(index.column());

  return (node_id << 32) | index_column;
}

std::optional<std::size_t>
CodeView::GetLineNumberFromBlockNumber(const TokenMap &token_map,
                                       const int &block_number) {

  auto block_entry_list_it =
      std::next(token_map.block_entry_list.begin(), block_number);

  if (block_entry_list_it >= token_map.block_entry_list.end()) {
    return std::nullopt;
  }

  const auto &block_entry = *block_entry_list_it;
  if (block_entry.line_number != 0) {
    return block_entry.line_number;
  }

  return std::nullopt;
}

std::optional<std::size_t>
CodeView::GetBlockNumberFromLineNumber(const TokenMap &token_map,
                                       const std::size_t &line_number) {

  auto line_num_to_block_num_map_it =
      token_map.line_num_to_block_num_map.find(line_number);
  if (line_num_to_block_num_map_it ==
      token_map.line_num_to_block_num_map.end()) {
    return std::nullopt;
  }

  const auto &block_number = line_num_to_block_num_map_it->second;
  return block_number;
}

std::optional<QModelIndex>
CodeView::GetQModelIndexFromTextCursor(const QAbstractItemModel &model,
                                       const TokenMap &token_map,
                                       const QTextCursor &cursor) {

  auto block_number = cursor.blockNumber();
  auto block_entry_list_it =
      std::next(token_map.block_entry_list.begin(), block_number);

  if (block_entry_list_it >= token_map.block_entry_list.end()) {
    return std::nullopt;
  }

  const auto &block_entry = *block_entry_list_it;

  const auto &block = cursor.block();
  auto block_position{block.position()};

  auto relative_cursor_pos = cursor.position() - block_position + 1;

  auto token_entry_list_it = std::find_if(
      block_entry.token_entry_list.begin(), block_entry.token_entry_list.end(),

      [&relative_cursor_pos](const auto &token_entry) -> bool {
        return (relative_cursor_pos >= token_entry.cursor_start &&
                relative_cursor_pos <= token_entry.cursor_end);
      });

  if (token_entry_list_it == block_entry.token_entry_list.end()) {
    return std::nullopt;
  }

  auto column_number =
      std::distance(block_entry.token_entry_list.begin(), token_entry_list_it);

  auto row_index = model.index(block_number, 0, QModelIndex());
  return model.index(0, static_cast<int>(column_number), row_index);
}

void CodeView::CreateTextDocumentLine(CodeView::TokenMap &token_map,
                                      QTextDocument &document,
                                      const CodeViewTheme &theme,
                                      const QModelIndex &row_index) {
  QTextCursor text_cursor(&document);

  auto block_number = row_index.row();
  auto text_block = document.findBlockByNumber(block_number);
  if (text_block.isValid()) {
    text_cursor.setPosition(text_block.position());
  } else {
    text_cursor.movePosition(QTextCursor::End);
  }

  text_cursor.beginEditBlock();

  const auto &model = *row_index.model();
  auto column_count = model.columnCount(row_index);

  TokenMap::BlockEntry block_entry;
  QTextCharFormat text_format;

  if (auto line_number_var = row_index.data(ICodeModel::LineNumberRole);
      line_number_var.isValid()) {
    block_entry.line_number = qvariant_cast<std::size_t>(line_number_var);
  }

  auto block_position{text_cursor.position()};
  bool contains_macro_expansion{false};

  for (int column_number{0}; column_number < column_count; ++column_number) {
    auto token_index = model.index(0, column_number, row_index);

    if (!contains_macro_expansion) {
      auto is_macro_expansion_var =
          token_index.data(ICodeModel::IsMacroExpansionRole);

      if (is_macro_expansion_var.isValid()) {
        contains_macro_expansion = qvariant_cast<bool>(is_macro_expansion_var);
      }
    }

    QString display_role;
    if (auto display_role_var = token_index.data(Qt::DisplayRole);
        display_role_var.isValid()) {
      display_role = display_role_var.toString();
    } else {
      continue;
    }

    auto entity_id_var = token_index.data(ICodeModel::TokenIdRole);
    Assert(entity_id_var.isValid(), "Invalid entity id");

    TokenMap::TokenEntry token_entry;
    token_entry.entity_id = qvariant_cast<RawEntityId>(entity_id_var);

    Assert(!display_role.contains("\n") && !display_role.contains("\r"),
           "The DisplayRole for entity " +
               std::to_string(token_entry.entity_id) + "contains newlines");

    if (auto related_entity_id_var =
            token_index.data(ICodeModel::RealRelatedEntityIdRole);
        related_entity_id_var.isValid()) {

      token_entry.related_entity_id =
          qvariant_cast<RawEntityId>(related_entity_id_var);

      if (token_entry.related_entity_id != kInvalidEntityId) {
        auto &related_entity_id_list =
            token_map
                .related_entity_to_entity_list[token_entry.related_entity_id];

        related_entity_id_list.push_back(token_entry.entity_id);
      }
    }

    token_entry.cursor_start = text_cursor.position() - block_position;
    token_entry.cursor_end =
        token_entry.cursor_start + static_cast<int>(display_role.size());

    block_entry.token_entry_list.push_back(std::move(token_entry));

    ConfigureTextFormatFromTheme(
        text_format, theme, token_index.data(ICodeModel::TokenCategoryRole));

    text_cursor.insertText(display_role, text_format);
  }

  block_entry.contains_macro_expansion = contains_macro_expansion;

  text_cursor.insertText("\n");
  text_cursor.endEditBlock();

  auto block_entry_list_it{token_map.block_entry_list.end()};
  if (static_cast<std::size_t>(block_number) !=
      token_map.block_entry_list.size()) {

    block_entry_list_it =
        std::next(token_map.block_entry_list.begin(), block_number);
  }

  token_map.block_entry_list.insert(block_entry_list_it,
                                    std::move(block_entry));
}

void CodeView::EraseTextDocumentLine(CodeView::TokenMap &token_map,
                                     QTextDocument &document,
                                     const int &block_number) {

  // Delete the block from the text document
  auto block_to_delete = document.findBlockByLineNumber(block_number);
  Assert(block_to_delete.isValid(), "Invalid block number");

  QTextCursor cursor(block_to_delete);
  cursor.select(QTextCursor::BlockUnderCursor);
  cursor.beginEditBlock();
  cursor.removeSelectedText();
  cursor.endEditBlock();

  // Delete the block entry
  auto block_entry_list_it =
      std::next(token_map.block_entry_list.begin(), block_number);

  Assert(block_entry_list_it < token_map.block_entry_list.end(),
         "Invalid block number");

  const auto &block_entry = *block_entry_list_it;
  for (const auto &token_entry : block_entry.token_entry_list) {
    if (token_entry.related_entity_id == kInvalidEntityId) {
      continue;
    }

    auto related_entity_to_entity_list_it =
        token_map.related_entity_to_entity_list.find(
            token_entry.related_entity_id);

    Assert(related_entity_to_entity_list_it !=
               token_map.related_entity_to_entity_list.end(),
           "Invalid related entity id");

    auto &related_entity_id_list = related_entity_to_entity_list_it->second;

    auto related_entity_id_list_it =
        std::find(related_entity_id_list.begin(), related_entity_id_list.end(),
                  token_entry.entity_id);

    if (related_entity_id_list_it != related_entity_id_list.end()) {
      related_entity_id_list.erase(related_entity_id_list_it);
    }

    if (related_entity_id_list.empty()) {
      token_map.related_entity_to_entity_list.erase(
          related_entity_to_entity_list_it);
    }
  }

  token_map.block_entry_list.erase(block_entry_list_it);
}

void CodeView::UpdateTokenDataLineNumbers(TokenMap &token_map) {
  auto max_block_number{token_map.block_entry_list.size()};

  for (std::size_t block_number{0}; block_number < max_block_number;
       ++block_number) {
    const auto &block_entry = token_map.block_entry_list[block_number];
    token_map.line_num_to_block_num_map.insert(
        {block_entry.line_number, block_number});

    token_map.highest_line_number =
        std::max(token_map.highest_line_number, block_entry.line_number);
  }
}

void CodeView::UpdateTokenMappings(TokenMap &token_map,
                                   const QTextDocument &document) {
  token_map.entity_cursor_range_map.clear();

  auto max_block_number{token_map.block_entry_list.size()};

  for (std::size_t block_number{0}; block_number < max_block_number;
       ++block_number) {

    const auto &block =
        document.findBlockByLineNumber(static_cast<int>(block_number));

    auto block_position = block.position();

    const auto &block_entry = token_map.block_entry_list[block_number];
    for (const auto &token_entry : block_entry.token_entry_list) {
      TokenMap::CursorRange cursor_range;
      cursor_range.start = block_position + token_entry.cursor_start;
      cursor_range.end = block_position + token_entry.cursor_end;

      QTextCursor text_cursor(block);

      text_cursor.setPosition(cursor_range.start, QTextCursor::MoveAnchor);
      text_cursor.setPosition(cursor_range.end, QTextCursor::KeepAnchor);

      token_map.entity_cursor_range_map.insert(
          {token_entry.entity_id, std::move(cursor_range)});
    }
  }
}

QTextDocument *CodeView::CreateTextDocument(
    CodeView::TokenMap &token_map, const QAbstractItemModel &model,
    const CodeViewTheme &theme,
    const std::optional<CreateTextDocumentProgressCallback>
        &opt_progress_callback) {

  token_map = {};

  auto document = new QTextDocument();
  auto document_layout = new QPlainTextDocumentLayout(document);
  document->setDocumentLayout(document_layout);

  auto row_count = model.rowCount();

  auto L_updateProgress = [&](const int &row_number) -> bool {
    if (!opt_progress_callback.has_value()) {
      return true;
    }

    const auto &progress_callback = opt_progress_callback.value();

    if (row_number == row_count) {
      static_cast<void>(progress_callback(100));
      return true;
    }

    if ((row_number % 100) != 0) {
      return true;
    }

    auto current_progress = (row_number * 100) / row_count;
    return progress_callback(static_cast<int>(current_progress));
  };

  for (int row_number{0}; row_number < row_count; ++row_number) {
    if (!L_updateProgress(row_number)) {
      break;
    }

    auto row_index = model.index(row_number, 0, QModelIndex());
    CreateTextDocumentLine(token_map, *document, theme, row_index);
  }

  UpdateTokenDataLineNumbers(token_map);
  UpdateTokenMappings(token_map, *document);

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

void CodeView::HighlightTokensForRelatedEntityID(
    const TokenMap &token_map, const QTextCursor &text_cursor,
    const QModelIndex &model_index,
    QList<QTextEdit::ExtraSelection> &selection_list,
    const CodeViewTheme &theme) {

  QVariant related_entity_id_var =
      model_index.data(ICodeModel::RealRelatedEntityIdRole);

  if (!related_entity_id_var.isValid()) {
    return;
  }

  auto related_entity_id = qvariant_cast<std::uint64_t>(related_entity_id_var);
  auto related_entity_to_entity_list_it =
      token_map.related_entity_to_entity_list.find(related_entity_id);

  if (related_entity_to_entity_list_it ==
      token_map.related_entity_to_entity_list.end()) {
    return;
  }

  const auto &related_entity_list = related_entity_to_entity_list_it->second;

  QTextEdit::ExtraSelection selection;

  for (const auto &related_entity : related_entity_list) {
    auto entity_cursor_range_map_it =
        token_map.entity_cursor_range_map.find(related_entity);

    if (entity_cursor_range_map_it == token_map.entity_cursor_range_map.end()) {
      continue;
    }

    const auto &absolute_cursor_range = entity_cursor_range_map_it->second;

    selection = {};
    selection.format.setBackground(theme.highlighted_entity_background_color);

    selection.cursor = text_cursor;
    selection.cursor.setPosition(absolute_cursor_range.start,
                                 QTextCursor::MoveMode::MoveAnchor);

    selection.cursor.setPosition(absolute_cursor_range.end,
                                 QTextCursor::MoveMode::KeepAnchor);

    selection_list.append(std::move(selection));
  }
}

void CodeView::OnRowsRemoved(const QModelIndex &parent, int first, int last) {
  Assert(!parent.isValid(), "The parent index should always be the model root");

  auto &document = *d->text_edit->document();

  // Once we delete a line, everything after it just moves up. Delete
  // the same line multiple times, if needed
  for (auto block_number{first}; block_number <= last; ++block_number) {
    EraseTextDocumentLine(d->token_map, document, first);
  }

  UpdateTokenDataLineNumbers(d->token_map);
  UpdateTokenMappings(d->token_map, document);
  StartDelayedDocumentUpdateSignal();
}

void CodeView::OnRowsInserted(const QModelIndex &parent, int first, int last) {
  Assert(!parent.isValid(), "The parent index should always be the model root");

  auto &document = *d->text_edit->document();

  for (auto block_number{first}; block_number <= last; ++block_number) {
    auto row_index = d->model->index(block_number, 0, QModelIndex());
    CreateTextDocumentLine(d->token_map, document, d->theme, row_index);
  }

  UpdateTokenDataLineNumbers(d->token_map);
  UpdateTokenMappings(d->token_map, document);
  StartDelayedDocumentUpdateSignal();
}

void CodeView::OnDataChange(const QModelIndex &, const QModelIndex &,
                            const QList<int> &roles) {

  if (roles.size() != 1 || roles[0] != Qt::BackgroundRole) {
    OnModelReset();
    return;
  }

  UpdateBaseExtraSelections();
  d->text_edit->viewport()->update();
}

void CodeView::OnModelReset() {
  d->version++;
  d->last_block = -1;
  d->search_widget->Deactivate();
  d->go_to_line_widget->Deactivate();
  d->opt_prev_hovered_model_index.reset();

  QProgressDialog progress(tr("Fetching code..."), tr("Abort"), 0, 100, this);
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
  UpdateBaseExtraSelections();
  UpdateTabStopDistance();

  // If there was a request to scroll to a line, but the document wasn't ready
  // at the time of the request, then enact the scroll now.
  if (d->deferred_scroll_to_line.has_value()) {
    auto line = d->deferred_scroll_to_line.value();
    d->deferred_scroll_to_line.reset();
    ScrollToLineNumber(line);
  }

  emit DocumentChanged();
}

void CodeView::DocumentChangedTimedSignal() {
  d->update_timer.stop();
  emit DocumentChanged();
}

void CodeView::OnGutterPaintEvent(QPaintEvent *event) {
  QPainter painter(d->gutter);
  painter.fillRect(event->rect(), d->theme.default_gutter_background);

  QTextBlock block = d->text_edit->firstVisibleBlock();
  int top = qRound(d->text_edit->blockBoundingGeometry(block)
                       .translated(d->text_edit->contentOffset())
                       .top());

  int bottom = top + qRound(d->text_edit->blockBoundingRect(block).height());

  auto right_line_num_margin = d->gutter->width() - (d->gutter->width() / 3);

  auto original_font{painter.font()};
  auto gutter_font{painter.font()};

  while (block.isValid() && top <= event->rect().bottom()) {
    if (block.isVisible() && bottom >= event->rect().top()) {
      auto block_number = block.blockNumber();

      auto opt_line_number =
          GetLineNumberFromBlockNumber(d->token_map, block_number);

      if (opt_line_number.has_value()) {
        auto block_entry_it =
            std::next(d->token_map.block_entry_list.begin(), block_number);

        Assert(
            block_entry_it != d->token_map.block_entry_list.end(),
            "Block number mismatch between the view and the block entry list");

        const auto &block_entry = *block_entry_it;
        gutter_font.setBold(block_entry.contains_macro_expansion);
        painter.setFont(gutter_font);

        if (block_entry.contains_macro_expansion) {
          painter.setPen(d->theme.ForegroundColor(TokenCategory::MACRO_NAME));
        } else {
          painter.setPen(d->theme.ForegroundColor(TokenCategory::LINE_NUMBER));
        }

        auto line_number = opt_line_number.value();
        painter.drawText(0, top, right_line_num_margin, fontMetrics().height(),
                         Qt::AlignRight, QString::number(line_number));
      }
    }

    block = block.next();
    top = bottom;
    bottom = top + qRound(d->text_edit->blockBoundingRect(block).height());
  }

  painter.setFont(original_font);
}

void CodeView::OnTextEditUpdateRequest(const QRect &rect, int dy) {
  if (dy) {
    d->gutter->scroll(0, dy);
  } else {
    d->gutter->update(0, rect.y(), d->gutter->width(), rect.height());
  }
}

void CodeView::HandleNewCursor(const QTextCursor &cursor) {
  auto extra_selections = d->extra_selection_list;

  QTextEdit::ExtraSelection selection;

  // Highlight the current line where the cursor is.
  selection.format.setBackground(d->theme.selected_line_background_color);
  selection.format.setProperty(QTextFormat::FullWidthSelection, true);
  selection.cursor = cursor;
  selection.cursor.clearSelection();
  extra_selections.prepend(selection);

  // Try to highlight all entities related to the entity on which the cursor
  // is hovering.
  //
  // NOTE(pag): We use `RealRelatedEntityIdRole` instead of the usual
  //            `RelatedEntityIdRole` because the code preview alters
  //            the related entity ID to be the token ID via a proxy model.
  auto opt_model_index =
      GetQModelIndexFromTextCursor(*d->model, d->token_map, cursor);

  if (opt_model_index.has_value()) {
    const auto &model_index = opt_model_index.value();

    HighlightTokensForRelatedEntityID(d->token_map, cursor, model_index,
                                      extra_selections, d->theme);

    // Tell users of the code view when the cursor moves.
    emit CursorMoved(model_index);
  }

  d->text_edit->setExtraSelections(extra_selections);
}

bool CodeView::ScrollToLineNumberInternal(unsigned line) {
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

void CodeView::SetZoom(const qreal &font_point_size) {
  auto font = this->font();
  font.setPointSizeF(font_point_size);

  setFont(font);
  d->text_edit->setFont(font);
  d->gutter->setFont(font);

  UpdateTabStopDistance();
  UpdateGutterWidth();
}

void CodeView::SetZoomDelta(const qreal &font_point_size_delta) {
  auto font = this->font();
  auto font_point_size = font.pointSizeF() + font_point_size_delta;
  if (font_point_size <= 1.0 || font_point_size >= 100) {
    return;
  }

  SetZoom(font_point_size);
}

void CodeView::StartDelayedDocumentUpdateSignal() {
  d->update_timer.start(100);
}

void CodeView::OnCursorMoved(void) {
  HandleNewCursor(d->text_edit->textCursor());
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
  d->go_to_line_widget->Activate(
      static_cast<unsigned>(d->token_map.highest_line_number));
}

void CodeView::OnGoToLine(unsigned line_number) {
  ScrollToLineNumber(line_number);
}

void CodeView::OnThemeChange(const QPalette &,
                             const CodeViewTheme &code_view_theme) {
  SetTheme(code_view_theme);
}

void CodeView::OnZoomIn() {
  SetZoomDelta(1.0);
}

void CodeView::OnZoomOut() {
  SetZoomDelta(-1.0);
}

void CodeView::OnResetZoom() {
  SetZoom(d->default_font_point_size);
}

void CodeView::OnEntityLocation(RawEntityId, unsigned line, unsigned) {
  if (line && !d->deferred_scroll_to_line.has_value()) {
    ScrollToLineNumber(line); 
  }
}

}  // namespace mx::gui
