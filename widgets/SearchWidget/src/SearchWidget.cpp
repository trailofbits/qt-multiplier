/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/GUI/Widgets/SearchWidget.h>

#include <multiplier/GUI/Managers/MediaManager.h>
#include <multiplier/GUI/Widgets/LineEditWidget.h>

#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPixmap>
#include <QAction>
#include <QRegularExpression>
#include <QPushButton>
#include <QShortcut>

namespace mx::gui {

struct SearchWidget::PrivateData final {
  Mode mode{Mode::Search};

  bool case_sensitive{false};
  bool whole_word{false};
  bool enable_regex{false};

  size_t search_result_count{};
  size_t current_search_result{};

  QIcon show_prev_result_icon;
  QPushButton *show_prev_result{nullptr};

  QIcon show_next_result_icon;
  QPushButton *show_next_result{nullptr};

  QIcon search_icon;
  QAction *search_icon_action{nullptr};

  QIcon enabled_case_sensitive_search;
  QIcon disabled_case_sensitive_search;
  QAction *case_sensitive_search_action{nullptr};

  QIcon enabled_regex_search;
  QIcon disabled_regex_search;
  QAction *regex_search_action{nullptr};

  QIcon enabled_whole_word_search;
  QIcon disabled_whole_word_search;
  QAction *whole_word_search_action{nullptr};

  LineEditWidget *search_input{nullptr};
  QLineEdit *search_input_error_display{nullptr};

  QShortcut *enable_search_shortcut{nullptr};
  QShortcut *disable_search_shortcut{nullptr};
  QShortcut *search_previous_shortcut{nullptr};
  QShortcut *search_next_shortcut{nullptr};

  SearchParameters search_parameters;
};

SearchWidget::~SearchWidget(void) {}

void SearchWidget::UpdateSearchResultCount(size_t search_result_count) {
  d->search_result_count = search_result_count;
  d->current_search_result = 0;

  d->show_next_result->setEnabled(d->search_result_count != 0);
  d->show_prev_result->setEnabled(d->search_result_count != 0);

  if (d->search_result_count == 0) {
    SetDisplayMessage(false, tr("No result found"));
    return;
  }

  ShowResult();
}

SearchWidget::SearchWidget(const MediaManager &media_manager, Mode mode,
                           QWidget *parent)
    : QWidget(parent),
      d(new PrivateData) {

  // It is important to have a valid parent because that's what
  // we use to scope the keyboard shortcuts
  Q_ASSERT(parent != nullptr);

  d->mode = mode;

  LoadIcons(media_manager);
  InitializeWidgets();
  InitializeKeyboardShortcuts(parent);

  connect(&media_manager, &MediaManager::IconsChanged,
          this, &SearchWidget::OnIconsChanged);
}

void SearchWidget::InitializeWidgets(void) {
  // The search layout contains the input box and all the buttons
  auto search_widget_layout = new QHBoxLayout();
  search_widget_layout->setContentsMargins(0, 0, 0, 0);
  search_widget_layout->setSpacing(0);

  d->search_input = new LineEditWidget(this);
  d->search_input->setClearButtonEnabled(true);
  d->search_input->setPlaceholderText(d->mode == Mode::Search ? tr("Search")
                                                              : tr("Filter"));
  search_widget_layout->addWidget(d->search_input, 1);
  search_widget_layout->addStretch();

  if (d->mode == Mode::Search) {
    d->show_prev_result = new QPushButton(d->show_prev_result_icon, "");
    d->show_prev_result->setEnabled(false);

    connect(d->show_prev_result, &QPushButton::clicked, this,
            &SearchWidget::OnShowPreviousResult);

    search_widget_layout->addWidget(d->show_prev_result);

    d->show_next_result = new QPushButton(d->show_next_result_icon, "");
    d->show_next_result->setEnabled(false);

    connect(d->show_next_result, &QPushButton::clicked, this,
            &SearchWidget::OnShowNextResult);

    search_widget_layout->addWidget(d->show_next_result);
  }

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
  d->search_icon_action = new QAction(QIcon(), tr("Search"));

  d->case_sensitive_search_action =
      new QAction(tr("Enable case sensitive search"));

  d->case_sensitive_search_action->setCheckable(true);
  d->case_sensitive_search_action->setChecked(false);

  d->search_input->addAction(d->case_sensitive_search_action,
                             QLineEdit::TrailingPosition);

  d->whole_word_search_action =
      new QAction(QIcon(), tr("Enable whole word search"));

  d->whole_word_search_action->setCheckable(true);
  d->whole_word_search_action->setChecked(false);

  d->search_input->addAction(d->whole_word_search_action,
                             QLineEdit::TrailingPosition);

  d->regex_search_action = new QAction(
      QIcon(), tr("Enable regex search"));

  d->regex_search_action->setCheckable(true);
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

  setVisible(false);
  UpdateIcons();
}

void SearchWidget::InitializeKeyboardShortcuts(QWidget *parent) {
  d->enable_search_shortcut =
      new QShortcut(QKeySequence::Find, parent, this, &SearchWidget::Activate,
                    Qt::WidgetWithChildrenShortcut);

  d->disable_search_shortcut =
      new QShortcut(QKeySequence::Cancel, this, this,
                    &SearchWidget::Disable, Qt::WidgetWithChildrenShortcut);

  if (d->mode == Mode::Search) {
    d->search_previous_shortcut = new QShortcut(
        QKeySequence::FindPrevious, parent, this,
        &SearchWidget::OnShowPreviousResult, Qt::WidgetWithChildrenShortcut);

    d->search_next_shortcut = new QShortcut(
        QKeySequence::FindNext, parent, this, &SearchWidget::OnShowNextResult,
        Qt::WidgetWithChildrenShortcut);

    connect(d->search_input, &QLineEdit::returnPressed, this,
            &SearchWidget::OnShowNextResult);
  }
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

void SearchWidget::ShowResult(void) {
  SetDisplayMessage(false, tr("Showing result ") +
                               QString::number(d->current_search_result + 1) +
                               tr(" of ") +
                               QString::number(d->search_result_count));

  emit ShowSearchResult(d->current_search_result);
}

void SearchWidget::UpdateIcons(void) {
  d->search_input->removeAction(d->search_icon_action);
  d->search_icon_action->setIcon(d->search_icon);
  d->search_input->addAction(d->search_icon_action, QLineEdit::LeadingPosition);

  d->case_sensitive_search_action->setIcon(d->disabled_case_sensitive_search);
  d->whole_word_search_action->setIcon(d->disabled_whole_word_search);
  d->regex_search_action->setIcon(d->disabled_regex_search);

  if (d->mode == Mode::Search) {
    d->show_next_result->setIcon(d->show_next_result_icon);
    d->show_prev_result->setIcon(d->show_prev_result_icon);
  }
}

void SearchWidget::OnSearchInputTextChanged(const QString &) {
  ClearDisplayMessage();

  if (d->enable_regex) {
    d->search_parameters.type = SearchParameters::Type::RegularExpression;
    d->search_parameters.case_sensitive = d->case_sensitive;
    d->search_parameters.whole_word = false;

    QRegularExpression::PatternOption options{
        QRegularExpression::NoPatternOption};

    if (!d->case_sensitive) {
      options = QRegularExpression::CaseInsensitiveOption;
    }

    auto search_pattern = d->search_input->text();
    d->search_parameters.pattern = search_pattern.toStdString();

    QRegularExpression regex(search_pattern, options);
    if (!regex.isValid()) {
      SetDisplayMessage(true, tr("Error: ") + regex.errorString());
      return;
    }

  } else {
    d->search_parameters.type = SearchParameters::Type::Text;
    d->search_parameters.case_sensitive = d->case_sensitive;
    d->search_parameters.whole_word = d->whole_word;
    d->search_parameters.pattern = d->search_input->text().toStdString();
  }

  d->search_result_count = 0;
  d->current_search_result = 0;

  emit SearchParametersChanged();
}

const SearchWidget::SearchParameters &SearchWidget::Parameters(void) const {
  return d->search_parameters;
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

void SearchWidget::OnShowPreviousResult(void) {
  if (d->mode != Mode::Search || d->search_result_count == 0) {
    return;
  }

  if (!isVisible()) {
    Activate();
    return;
  }

  ClearDisplayMessage();

  if (d->current_search_result == 0) {
    d->current_search_result = d->search_result_count - 1;
  } else {
    d->current_search_result -= 1;
  }

  ShowResult();
}

void SearchWidget::OnShowNextResult(void) {
  if (d->mode != Mode::Search || d->search_result_count == 0) {
    return;
  }

  if (!isVisible()) {
    Activate();
    return;
  }

  ClearDisplayMessage();

  if (d->current_search_result == d->search_result_count - 1) {
    d->current_search_result = 0;
  } else {
    d->current_search_result += 1;
  }

  ShowResult();
}

//! Loads the required icons from the resources
void SearchWidget::LoadIcons(const MediaManager &media_manager) {
  d->search_icon = media_manager.Pixmap("com.trailofbits.icon.Search");

  d->enabled_case_sensitive_search = media_manager.Pixmap(
      "com.trailofbits.icon.CaseSensitiveSearch",
      ITheme::IconStyle::HIGHLIGHTED);

  d->disabled_case_sensitive_search = media_manager.Pixmap(
      "com.trailofbits.icon.CaseSensitiveSearch");

  d->enabled_regex_search = media_manager.Pixmap(
      "com.trailofbits.icon.RegexSearch", ITheme::IconStyle::HIGHLIGHTED);

  d->disabled_regex_search = media_manager.Pixmap(
      "com.trailofbits.icon.RegexSearch");

  d->enabled_whole_word_search = media_manager.Pixmap(
      "com.trailofbits.icon.WholeWordSearch", ITheme::IconStyle::HIGHLIGHTED);

  d->disabled_whole_word_search = media_manager.Pixmap(
      "com.trailofbits.icon.WholeWordSearch");

  d->show_prev_result_icon = media_manager.Pixmap(
      "com.trailofbits.icon.PreviousSearchResult");

  d->show_next_result_icon = media_manager.Pixmap(
      "com.trailofbits.icon.NextSearchResult");
}

void SearchWidget::OnIconsChanged(const MediaManager &media_manager) {
  LoadIcons(media_manager);
  UpdateIcons();
}

void SearchWidget::Activate(void) {
  setVisible(true);

  d->search_input->setFocus();
  d->search_input->clear();

  d->search_input_error_display->clear();
  d->search_input_error_display->setVisible(false);

  d->search_result_count = 0;
  d->current_search_result = 0;

  emit Activated();
}

void SearchWidget::Deactivate(void) {
  setVisible(false);

  d->search_input->clear();

  d->search_input_error_display->clear();
  d->search_input_error_display->setVisible(false);

  d->search_result_count = 0;
  d->current_search_result = 0;

  emit Deactivated();
}

//! Disable this wiget.
void SearchWidget::Disable(void) {
  Deactivate();
  if (auto pw = dynamic_cast<QWidget *>(parent())) {
    pw->setFocus(Qt::OtherFocusReason);
  }
}

}  // namespace mx::gui
