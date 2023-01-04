/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "SearchWidget.h"

#include <QIcon>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPixmap>
#include <QAction>
#include <QRegularExpression>
#include <QTimer>
#include <QPushButton>

namespace mx::gui {

struct SearchWidget::PrivateData final {
  QPlainTextEdit *text_edit{nullptr};

  bool case_sensitive{false};
  bool whole_word{false};
  bool enable_regex{false};

  QIcon show_prev_result_icon;
  QPushButton *show_prev_result{nullptr};

  QIcon show_next_result_icon;
  QPushButton *show_next_result{nullptr};

  QIcon search_icon;

  QIcon enabled_case_sensitive_search;
  QIcon disabled_case_sensitive_search;
  QAction *case_sensitive_search_action{nullptr};

  QIcon enabled_regex_search;
  QIcon disabled_regex_search;
  QAction *regex_search_action{nullptr};

  QIcon enabled_whole_word_search;
  QIcon disabled_whole_word_search;
  QAction *whole_word_search_action{nullptr};

  QLineEdit *search_input{nullptr};
  QLineEdit *search_input_error_display{nullptr};

  QTimer signal_timer;

  std::vector<std::pair<int, int>> result_list;
  std::optional<std::size_t> opt_active_result;
};

SearchWidget::SearchWidget(QPlainTextEdit *text_edit, QWidget *parent)
    : QWidget(parent),
      d(new PrivateData) {

  d->text_edit = text_edit;

  LoadIcons();
  InitializeWidgets();
}

SearchWidget::~SearchWidget() {}

void SearchWidget::Activate() {
  setVisible(true);

  d->search_input->setFocus();
  d->search_input->clear();

  d->search_input_error_display->clear();
  d->search_input_error_display->setVisible(false);
}

void SearchWidget::Deactivate() {
  setVisible(false);

  d->search_input->clear();

  d->search_input_error_display->clear();
  d->search_input_error_display->setVisible(false);
}

void SearchWidget::LoadIcons() {
  d->search_icon = QIcon(":/CodeView/search_icon");

  d->enabled_case_sensitive_search =
      QIcon(":/CodeView/search_icon_case_sensitive_on");

  d->disabled_case_sensitive_search =
      QIcon(":/CodeView/search_icon_case_sensitive_off");

  d->enabled_regex_search = QIcon(":/CodeView/search_icon_regex_on");
  d->disabled_regex_search = QIcon(":/CodeView/search_icon_regex_off");

  d->enabled_whole_word_search = QIcon(":/CodeView/search_icon_whole_word_on");
  d->disabled_whole_word_search =
      QIcon(":/CodeView/search_icon_whole_word_off");

  d->show_prev_result_icon = QIcon(":/CodeView/show_prev_result");
  d->show_next_result_icon = QIcon(":/CodeView/show_next_result");
}

void SearchWidget::InitializeWidgets() {
  // The search layout contains the input box and all the buttons
  auto search_widget_layout = new QHBoxLayout();
  search_widget_layout->setContentsMargins(0, 0, 0, 0);
  search_widget_layout->setSpacing(0);

  d->search_input = new QLineEdit();
  d->search_input->setClearButtonEnabled(true);
  d->search_input->setPlaceholderText(tr("Search"));
  search_widget_layout->addWidget(d->search_input);

  d->show_prev_result = new QPushButton(d->show_prev_result_icon, "");
  d->show_prev_result->setEnabled(false);

  connect(d->show_prev_result, &QPushButton::clicked, this,
          &SearchWidget::OnShowPrevResult);

  search_widget_layout->addWidget(d->show_prev_result);

  d->show_next_result = new QPushButton(d->show_next_result_icon, "");
  d->show_next_result->setEnabled(false);

  connect(d->show_next_result, &QPushButton::clicked, this,
          &SearchWidget::OnShowNextResult);

  search_widget_layout->addWidget(d->show_next_result);

  // The main layout contains the search layout and the error display
  d->search_input_error_display = new QLineEdit();
  d->search_input_error_display->setVisible(false);

  auto main_layout = new QVBoxLayout();
  main_layout->setContentsMargins(0, 0, 0, 0);
  main_layout->setSpacing(0);

  main_layout->addWidget(d->search_input_error_display);
  main_layout->addItem(search_widget_layout);

  setLayout(main_layout);

  // Setup the input box
  auto search_icon_action = new QAction(d->search_icon, tr("Search"));

  d->search_input->addAction(search_icon_action, QLineEdit::LeadingPosition);

  d->case_sensitive_search_action =
      new QAction(tr("Enable case sensitive search"));

  d->case_sensitive_search_action->setCheckable(true);
  d->case_sensitive_search_action->setIcon(d->disabled_case_sensitive_search);
  d->case_sensitive_search_action->setChecked(false);

  d->search_input->addAction(d->case_sensitive_search_action,
                             QLineEdit::TrailingPosition);

  d->whole_word_search_action =
      new QAction(QPixmap(":/CodeView/search_icon_whole_word"),
                  tr("Enable whole word search"));

  d->whole_word_search_action->setCheckable(true);
  d->whole_word_search_action->setIcon(d->disabled_whole_word_search);
  d->whole_word_search_action->setChecked(false);

  d->search_input->addAction(d->whole_word_search_action,
                             QLineEdit::TrailingPosition);

  d->regex_search_action = new QAction(QPixmap(":/CodeView/search_icon_regex"),
                                       tr("Enable regex search"));

  d->regex_search_action->setCheckable(true);
  d->regex_search_action->setIcon(d->disabled_regex_search);
  d->regex_search_action->setChecked(false);

  d->search_input->addAction(d->regex_search_action,
                             QLineEdit::TrailingPosition);

  // Connect the signals
  connect(d->search_input, &QLineEdit::textChanged, this,
          &SearchWidget::OnSearchInputTextChanged);

  connect(d->case_sensitive_search_action, &QAction::toggled, this,
          &SearchWidget::OnCaseSensitiveSearchOptionToggled);

  connect(d->whole_word_search_action, &QAction::toggled, this,
          &SearchWidget::OnWholeWordSearchOptionToggled);

  connect(d->regex_search_action, &QAction::toggled, this,
          &SearchWidget::OnRegexSearchOptionToggled);

  connect(&d->signal_timer, &QTimer::timeout, this,
          &SearchWidget::OnTextSearch);
}

void SearchWidget::SetDisplayMessage(bool error, const QString &message) {
  d->search_input_error_display->setText(message);
  d->search_input_error_display->setVisible(true);

  auto palette = this->palette();
  palette.setColor(QPalette::Base, error ? palette.alternateBase().color()
                                         : palette.base().color());

  d->search_input_error_display->setPalette(palette);
}

void SearchWidget::ClearDisplayMessage() {
  d->search_input_error_display->clear();
  d->search_input_error_display->setVisible(false);
}

void SearchWidget::EnableNavigation(bool enable) {
  d->show_prev_result->setEnabled(enable);
  d->show_next_result->setEnabled(enable);
}

void SearchWidget::OnSearchInputTextChanged(const QString &text) {
  ClearDisplayMessage();
  d->signal_timer.stop();

  if (text.length() == 0) {
    return;
  }

  if (text.length() <= 2) {
    SetDisplayMessage(true, tr("The search pattern is too short"));
    return;
  }

  d->signal_timer.start(std::chrono::milliseconds(500));
}

void SearchWidget::OnCaseSensitiveSearchOptionToggled(bool checked) {
  d->case_sensitive = checked;
  OnSearchInputTextChanged(d->search_input->text());

  auto &icon = (d->case_sensitive ? d->enabled_case_sensitive_search
                                  : d->disabled_case_sensitive_search);

  d->case_sensitive_search_action->setIcon(icon);
}

void SearchWidget::OnWholeWordSearchOptionToggled(bool checked) {
  d->whole_word = checked;
  OnSearchInputTextChanged(d->search_input->text());

  auto &icon = (d->whole_word ? d->enabled_whole_word_search
                              : d->disabled_whole_word_search);

  d->whole_word_search_action->setIcon(icon);

  if (d->whole_word && d->regex_search_action->isChecked()) {
    d->regex_search_action->setChecked(false);
  }
}

void SearchWidget::OnRegexSearchOptionToggled(bool checked) {
  d->enable_regex = checked;
  OnSearchInputTextChanged(d->search_input->text());

  auto &icon =
      (d->enable_regex ? d->enabled_regex_search : d->disabled_regex_search);

  d->regex_search_action->setIcon(icon);

  if (d->enable_regex && d->whole_word_search_action->isChecked()) {
    d->whole_word_search_action->setChecked(false);
  }
}

void SearchWidget::OnTextSearch() {
  d->signal_timer.stop();

  auto search_pattern = d->search_input->text();
  auto contents = d->text_edit->toPlainText();

  d->result_list.clear();
  d->opt_active_result = std::nullopt;

  if (d->enable_regex) {
    QRegularExpression::PatternOption options{
        QRegularExpression::NoPatternOption};

    if (!d->case_sensitive) {
      options = QRegularExpression::CaseInsensitiveOption;
    }

    QRegularExpression regex(search_pattern, options);
    if (!regex.isValid()) {
      SetDisplayMessage(true, tr("Error: ") + regex.errorString());
      return;
    }

    auto match_iterator = regex.globalMatch(contents);

    while (match_iterator.hasNext()) {
      const auto &match = match_iterator.next();

      for (int captured_index = 0; captured_index <= match.lastCapturedIndex();
           ++captured_index) {
        auto start_cursor = match.capturedStart(captured_index);
        auto end_cursor = match.capturedEnd(captured_index);

        d->result_list.push_back(std::make_pair(start_cursor, end_cursor));
      }
    }

  } else {
  }

  EnableNavigation(!d->result_list.empty());

  SetDisplayMessage(
      false,
      tr("Found ") + QString::number(d->result_list.size()) + tr(" results"));
}

void SearchWidget::OnShowPrevResult() {
  std::size_t active_result{};
  if (d->opt_active_result.has_value()) {
    active_result = d->opt_active_result.value();

    if (active_result == 0) {
      active_result = d->result_list.size() - 1;
    } else {
      --active_result;
    }
  }

  SetDisplayMessage(false, tr("Showing result ") +
                               QString::number(active_result + 1) + tr(" of ") +
                               QString::number(d->result_list.size()));

  const auto &result = d->result_list[active_result];

  auto text_cursor = d->text_edit->textCursor();
  text_cursor.setPosition(result.first, QTextCursor::MoveMode::MoveAnchor);
  text_cursor.setPosition(result.second, QTextCursor::MoveMode::KeepAnchor);

  d->text_edit->moveCursor(QTextCursor::End);
  d->text_edit->setTextCursor(text_cursor);
  d->text_edit->centerCursor();

  d->opt_active_result = active_result;
}

void SearchWidget::OnShowNextResult() {
  std::size_t active_result{};
  if (d->opt_active_result.has_value()) {
    active_result = d->opt_active_result.value();

    if (active_result == d->result_list.size()) {
      active_result = 0;
    } else {
      ++active_result;
    }
  }

  SetDisplayMessage(false, tr("Showing result ") +
                               QString::number(active_result + 1) + tr(" of ") +
                               QString::number(d->result_list.size()));

  const auto &result = d->result_list[active_result];

  auto text_cursor = d->text_edit->textCursor();
  text_cursor.setPosition(result.first, QTextCursor::MoveMode::MoveAnchor);
  text_cursor.setPosition(result.second, QTextCursor::MoveMode::KeepAnchor);

  d->text_edit->moveCursor(QTextCursor::End);
  d->text_edit->setTextCursor(text_cursor);
  d->text_edit->centerCursor();

  d->opt_active_result = active_result;
}

}  // namespace mx::gui
