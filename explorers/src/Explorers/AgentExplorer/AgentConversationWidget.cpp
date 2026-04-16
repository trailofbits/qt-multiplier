// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "AgentConversationWidget.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

#include <multiplier/GUI/Interfaces/ITheme.h>
#include <multiplier/GUI/Managers/AgentMessage.h>
#include <multiplier/GUI/Managers/ThemeManager.h>

namespace mx::gui {

struct AgentConversationWidget::PrivateData {
  ThemeManager &theme_manager;

  QScrollArea *scroll_area{nullptr};
  QWidget *messages_container{nullptr};
  QVBoxLayout *messages_layout{nullptr};
  QPlainTextEdit *input_edit{nullptr};
  QPushButton *send_button{nullptr};
  QLabel *token_label{nullptr};

  // Theme colors.
  QColor user_bg;
  QColor assistant_bg;
  QColor system_fg;
  QColor tool_bg;
  QColor text_fg;

  QFrame *suggestion_frame{nullptr};
  QLabel *suggestion_text{nullptr};
  QLabel *suggestion_hint{nullptr};
  QPushButton *suggestion_more_btn{nullptr};
  QString current_suggestion;
  QStringList current_alternatives;

  int total_prompt_tokens{0};
  int total_completion_tokens{0};
  bool enter_to_send{true};

  explicit PrivateData(ThemeManager &tm)
      : theme_manager(tm) {}
};

AgentConversationWidget::~AgentConversationWidget(void) {}

void AgentConversationWidget::setEnterToSend(bool enabled) {
  d->enter_to_send = enabled;
}

AgentConversationWidget::AgentConversationWidget(ThemeManager &theme_manager,
                                                  QWidget *parent)
    : QWidget(parent),
      d(new PrivateData(theme_manager)) {

  auto *main_layout = new QVBoxLayout(this);
  main_layout->setContentsMargins(0, 0, 0, 0);
  main_layout->setSpacing(4);

  // Scrollable messages area.
  d->scroll_area = new QScrollArea(this);
  d->scroll_area->setWidgetResizable(true);
  d->scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  d->scroll_area->setFrameShape(QFrame::NoFrame);

  d->messages_container = new QWidget;
  d->messages_layout = new QVBoxLayout(d->messages_container);
  d->messages_layout->setContentsMargins(8, 8, 8, 8);
  d->messages_layout->setSpacing(8);
  d->messages_layout->addStretch();

  d->scroll_area->setWidget(d->messages_container);
  main_layout->addWidget(d->scroll_area, 1);

  // Suggestion box.
  d->suggestion_frame = new QFrame(this);
  d->suggestion_frame->setFrameShape(QFrame::StyledPanel);
  d->suggestion_frame->setStyleSheet(
      QStringLiteral("QFrame { background-color: palette(alternateBase); "
                     "border: 1px solid palette(mid); border-radius: 4px; }"));
  auto *sug_layout = new QHBoxLayout(d->suggestion_frame);
  sug_layout->setContentsMargins(8, 6, 8, 6);

  d->suggestion_text = new QLabel(d->suggestion_frame);
  d->suggestion_text->setWordWrap(true);
  sug_layout->addWidget(d->suggestion_text, 1);

  d->suggestion_hint = new QLabel(tr("Tab to accept"), d->suggestion_frame);
  auto hint_font = d->suggestion_hint->font();
  hint_font.setPointSize(hint_font.pointSize() - 1);
  d->suggestion_hint->setFont(hint_font);
  d->suggestion_hint->setStyleSheet(
      QStringLiteral("color: palette(mid);"));
  sug_layout->addWidget(d->suggestion_hint);

  d->suggestion_more_btn = new QPushButton(tr("more"), d->suggestion_frame);
  d->suggestion_more_btn->setFlat(true);
  sug_layout->addWidget(d->suggestion_more_btn);

  d->suggestion_frame->setVisible(false);
  main_layout->addWidget(d->suggestion_frame);

  // Input area.
  auto *input_layout = new QHBoxLayout;
  input_layout->setContentsMargins(4, 0, 4, 4);
  input_layout->setSpacing(4);

  d->input_edit = new QPlainTextEdit(this);
  d->input_edit->setPlaceholderText(tr("Type a message..."));
  d->input_edit->setMaximumHeight(80);
  d->input_edit->setTabChangesFocus(true);
  input_layout->addWidget(d->input_edit, 1);

  d->send_button = new QPushButton(tr("Send"), this);
  d->send_button->setFixedWidth(60);
  input_layout->addWidget(d->send_button, 0, Qt::AlignBottom);

  main_layout->addLayout(input_layout);

  // Token counter.
  d->token_label = new QLabel(tr("Tokens: 0 in / 0 out ($0.00)"), this);
  d->token_label->setContentsMargins(8, 0, 8, 4);
  auto font = d->token_label->font();
  font.setPointSize(font.pointSize() - 1);
  d->token_label->setFont(font);
  main_layout->addWidget(d->token_label);

  // Connections.
  connect(d->send_button, &QPushButton::clicked,
          this, &AgentConversationWidget::onSendClicked);
  connect(&theme_manager, &ThemeManager::ThemeChanged,
          this, &AgentConversationWidget::onThemeChanged);

  // Suggestion more button.
  connect(d->suggestion_more_btn, &QPushButton::clicked, this, [this] {
    if (d->current_alternatives.isEmpty()) {
      return;
    }
    auto *menu = new QMenu(this);
    for (const auto &alt : d->current_alternatives) {
      auto *action = menu->addAction(alt);
      connect(action, &QAction::triggered, this, [this, alt] {
        d->input_edit->setPlainText(alt);
        clearSuggestion();
      });
    }
    menu->popup(d->suggestion_more_btn->mapToGlobal(
        d->suggestion_more_btn->rect().bottomLeft()));
  });

  // Event filter on input for Tab-to-accept.
  d->input_edit->installEventFilter(this);

  // Default suggestion.
  showSuggestion(tr("List all source files and create a task board for "
                    "auditing this codebase."));

  applyThemeColors();
}

void AgentConversationWidget::addMessage(const AgentMessage &msg) {
  addMessageBubble(msg.role, msg.content, msg.tool_name,
                   msg.tool_args, msg.tool_result);
}

void AgentConversationWidget::updateTokens(int prompt_tokens,
                                            int completion_tokens,
                                            double cost_usd) {
  d->total_prompt_tokens = prompt_tokens;
  d->total_completion_tokens = completion_tokens;

  double total_cost;
  if (cost_usd >= 0.0) {
    // Use authoritative cost from the rates table.
    total_cost = cost_usd;
  } else {
    // Estimate cost using typical API pricing (per 1M tokens).
    // Input: ~$3/M, Output: ~$15/M (approximate mid-range).
    double cost_in = d->total_prompt_tokens * 3.0 / 1000000.0;
    double cost_out = d->total_completion_tokens * 15.0 / 1000000.0;
    total_cost = cost_in + cost_out;
  }

  d->token_label->setText(
      tr("Tokens: %L1 in / %L2 out ($%3)")
          .arg(d->total_prompt_tokens)
          .arg(d->total_completion_tokens)
          .arg(total_cost, 0, 'f', 2));
}

void AgentConversationWidget::clear(void) {
  // Remove all message widgets (keep the stretch at index 0).
  while (d->messages_layout->count() > 1) {
    auto *item = d->messages_layout->takeAt(1);
    if (item->widget()) {
      delete item->widget();
    }
    delete item;
  }
  d->total_prompt_tokens = 0;
  d->total_completion_tokens = 0;
  d->token_label->setText(tr("Tokens: 0 in / 0 out ($0.00)"));
}

void AgentConversationWidget::onSendClicked(void) {
  auto text = d->input_edit->toPlainText().trimmed();
  if (text.isEmpty()) {
    return;
  }
  d->input_edit->clear();
  clearSuggestion();
  emit sendMessageRequested(text);
}

void AgentConversationWidget::onThemeChanged(const ThemeManager &) {
  applyThemeColors();
}

void AgentConversationWidget::addMessageBubble(
    const QString &role, const QString &content, const QString &tool_name,
    const QJsonObject &tool_args, const QJsonObject &tool_result) {

  auto *frame = new QFrame(d->messages_container);
  frame->setFrameShape(QFrame::StyledPanel);
  frame->setFrameShadow(QFrame::Plain);
  auto *frame_layout = new QVBoxLayout(frame);
  frame_layout->setContentsMargins(8, 6, 8, 6);
  frame_layout->setSpacing(4);

  auto make_label = [&](const QString &text, bool mono = false) -> QLabel * {
    auto *label = new QLabel(frame);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    label->setText(text);
    if (mono) {
      auto f = label->font();
      f.setFamily(QStringLiteral("monospace"));
      f.setPointSize(f.pointSize() - 1);
      label->setFont(f);
    }
    return label;
  };

  if (role == QStringLiteral("user")) {
    frame->setStyleSheet(
        QStringLiteral("QFrame { background-color: %1; border-radius: 8px; }")
            .arg(d->user_bg.name()));
    auto *label = make_label(content);
    label->setAlignment(Qt::AlignRight);
    frame_layout->addWidget(label);

    // Right-align user messages.
    auto *wrapper = new QHBoxLayout;
    wrapper->addStretch(1);
    wrapper->addWidget(frame, 3);
    d->messages_layout->addLayout(wrapper);

  } else if (role == QStringLiteral("assistant")) {
    frame->setStyleSheet(
        QStringLiteral("QFrame { background-color: %1; border-radius: 8px; }")
            .arg(d->assistant_bg.name()));
    auto *label = make_label(content);
    frame_layout->addWidget(label);

    auto *wrapper = new QHBoxLayout;
    wrapper->addWidget(frame, 3);
    wrapper->addStretch(1);
    d->messages_layout->addLayout(wrapper);

  } else if (role == QStringLiteral("tool_call")) {
    frame->setStyleSheet(
        QStringLiteral("QFrame { background-color: %1; border-radius: 4px; }")
            .arg(d->tool_bg.name()));

    // Collapsible header.
    auto *toggle_btn = new QPushButton(
        QStringLiteral("Tool: %1").arg(tool_name), frame);
    toggle_btn->setFlat(true);
    toggle_btn->setStyleSheet(
        QStringLiteral("QPushButton { text-align: left; font-weight: bold; }"));
    frame_layout->addWidget(toggle_btn);

    // Detail widget (collapsed by default).
    auto *detail = new QWidget(frame);
    detail->setVisible(false);
    auto *detail_layout = new QVBoxLayout(detail);
    detail_layout->setContentsMargins(4, 0, 4, 0);

    if (!tool_args.isEmpty()) {
      auto args_str = QString::fromUtf8(
          QJsonDocument(tool_args).toJson(QJsonDocument::Indented));
      auto *args_label = make_label(QStringLiteral("Args: ") + args_str, true);
      detail_layout->addWidget(args_label);
    }
    frame_layout->addWidget(detail);

    connect(toggle_btn, &QPushButton::clicked, detail,
            [detail] { detail->setVisible(!detail->isVisible()); });

    auto *wrapper = new QHBoxLayout;
    wrapper->addWidget(frame, 3);
    wrapper->addStretch(1);
    d->messages_layout->addLayout(wrapper);

  } else if (role == QStringLiteral("tool_result")) {
    frame->setStyleSheet(
        QStringLiteral("QFrame { background-color: %1; border-radius: 4px; }")
            .arg(d->tool_bg.name()));

    auto *toggle_btn = new QPushButton(
        QStringLiteral("Result: %1").arg(tool_name), frame);
    toggle_btn->setFlat(true);
    toggle_btn->setStyleSheet(
        QStringLiteral("QPushButton { text-align: left; }"));
    frame_layout->addWidget(toggle_btn);

    auto *detail = new QWidget(frame);
    detail->setVisible(false);
    auto *detail_layout = new QVBoxLayout(detail);
    detail_layout->setContentsMargins(4, 0, 4, 0);

    auto result_str = content.left(2000);
    auto *result_label = make_label(result_str, true);
    detail_layout->addWidget(result_label);

    frame_layout->addWidget(detail);

    connect(toggle_btn, &QPushButton::clicked, detail,
            [detail] { detail->setVisible(!detail->isVisible()); });

    auto *wrapper = new QHBoxLayout;
    wrapper->addWidget(frame, 3);
    wrapper->addStretch(1);
    d->messages_layout->addLayout(wrapper);

  } else {
    // System or unknown role -- centered, italic.
    frame->setStyleSheet(
        QStringLiteral("QFrame { border: none; }"));
    auto *label = make_label(content);
    label->setAlignment(Qt::AlignCenter);
    auto f = label->font();
    f.setItalic(true);
    label->setFont(f);
    label->setStyleSheet(
        QStringLiteral("color: %1;").arg(d->system_fg.name()));
    frame_layout->addWidget(label);
    d->messages_layout->addWidget(frame);
  }

  scrollToBottom();
}

void AgentConversationWidget::showSuggestion(
    const QString &suggestion, const QStringList &alternatives) {
  d->current_suggestion = suggestion;
  d->current_alternatives = alternatives;
  d->suggestion_text->setText(suggestion);
  d->suggestion_more_btn->setVisible(!alternatives.isEmpty());
  d->suggestion_frame->setVisible(true);
}

void AgentConversationWidget::clearSuggestion(void) {
  d->current_suggestion.clear();
  d->current_alternatives.clear();
  d->suggestion_frame->setVisible(false);
}

bool AgentConversationWidget::eventFilter(QObject *obj, QEvent *event) {
  if (obj == d->input_edit && event->type() == QEvent::KeyPress) {
    auto *key_event = static_cast<QKeyEvent *>(event);

    // Enter/Shift+Enter behavior depends on enter_to_send setting.
    if (key_event->key() == Qt::Key_Return ||
        key_event->key() == Qt::Key_Enter) {
      bool has_shift = key_event->modifiers() & Qt::ShiftModifier;
      bool should_send = d->enter_to_send ? !has_shift : has_shift;
      if (should_send) {
        onSendClicked();
        return true;
      }
    }

    // Tab accepts suggestion.
    if (key_event->key() == Qt::Key_Tab &&
        d->suggestion_frame->isVisible() &&
        !d->current_suggestion.isEmpty()) {
      d->input_edit->setPlainText(d->current_suggestion);
      auto cursor = d->input_edit->textCursor();
      cursor.movePosition(QTextCursor::End);
      d->input_edit->setTextCursor(cursor);
      auto suggestion = d->current_suggestion;
      clearSuggestion();
      emit suggestionAccepted(suggestion);
      return true;
    }

    // Typing dismisses suggestion.
    if (d->suggestion_frame->isVisible() &&
        d->input_edit->toPlainText().isEmpty() &&
        key_event->key() != Qt::Key_Tab &&
        key_event->key() != Qt::Key_Shift &&
        key_event->key() != Qt::Key_Control &&
        key_event->key() != Qt::Key_Alt &&
        key_event->key() != Qt::Key_Meta) {
      clearSuggestion();
    }
  }
  return QWidget::eventFilter(obj, event);
}

void AgentConversationWidget::scrollToBottom(void) {
  QTimer::singleShot(0, this, [this] {
    auto *bar = d->scroll_area->verticalScrollBar();
    bar->setValue(bar->maximum());
  });
}

void AgentConversationWidget::applyThemeColors(void) {
  auto theme = d->theme_manager.Theme();
  if (!theme) {
    d->user_bg = QColor(0x3B, 0x82, 0xF6, 0x40);
    d->assistant_bg = QColor(0x6B, 0x72, 0x80, 0x30);
    d->system_fg = QColor(0x9C, 0xA3, 0xAF);
    d->tool_bg = QColor(0x37, 0x41, 0x51, 0x40);
    d->text_fg = QColor(0xFF, 0xFF, 0xFF);
    return;
  }

  auto palette = theme->Palette();
  auto text = palette.color(QPalette::Text);
  auto highlight = palette.color(QPalette::Highlight);

  d->text_fg = text;
  d->system_fg = palette.color(QPalette::PlaceholderText);

  // User messages: accent-tinted.
  d->user_bg = highlight;
  d->user_bg.setAlpha(60);

  // Assistant messages: subtle contrast.
  d->assistant_bg = text;
  d->assistant_bg.setAlpha(20);

  // Tool calls/results.
  d->tool_bg = palette.color(QPalette::AlternateBase);
}

}  // namespace mx::gui
