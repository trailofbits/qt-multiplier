/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "EntityExplorer.h"
#include "EntityExplorerItemDelegate.h"
#include "CategoryComboBox.h"

#include <multiplier/GUI/Assert.h>
#include <multiplier/GUI/ThemeManager.h>

#include <QBrush>
#include <QCheckBox>
#include <QLineEdit>
#include <QListView>
#include <QMouseEvent>
#include <QPalette>
#include <QSortFilterProxyModel>
#include <QVBoxLayout>
#include <QRadioButton>

namespace mx::gui {

struct EntityExplorer::PrivateData final {
  ISearchWidget *filter_widget{nullptr};

  IEntityExplorerModel *model{nullptr};
  QListView *list_view{nullptr};

  QLineEdit *search_input{nullptr};
  QRadioButton *exact_match_radio{nullptr};
  QRadioButton *containing_radio{nullptr};
};

EntityExplorer::~EntityExplorer() {}

IEntityExplorerModel *EntityExplorer::Model() {
  return d->model;
}

EntityExplorer::EntityExplorer(IEntityExplorerModel *model, QWidget *parent,
                               IGlobalHighlighter *global_highlighter)
    : IEntityExplorer(parent),
      d(new PrivateData) {

  InitializeWidgets();
  InstallModel(model, global_highlighter);
}

void EntityExplorer::InitializeWidgets() {
  d->list_view = new QListView(this);
  d->list_view->setSelectionMode(
      QAbstractItemView::SelectionMode::SingleSelection);

  d->list_view->setSelectionBehavior(
      QAbstractItemView::SelectionBehavior::SelectRows);

  d->filter_widget = ISearchWidget::Create(ISearchWidget::Mode::Filter, this);
  connect(d->filter_widget, &ISearchWidget::SearchParametersChanged, this,
          &EntityExplorer::OnSearchParametersChange);

  auto search_parameters_layout = new QVBoxLayout();

  d->search_input = new QLineEdit(this);
  d->search_input->setClearButtonEnabled(true);
  d->search_input->setPlaceholderText(tr("Search"));

  connect(d->search_input, &QLineEdit::textChanged, this,
          &EntityExplorer::QueryParametersChanged);

  search_parameters_layout->addWidget(d->search_input);

  auto query_mode_layout = new QHBoxLayout();
  d->exact_match_radio = new QRadioButton(tr("Exact match"), this);
  query_mode_layout->addWidget(d->exact_match_radio);

  d->exact_match_radio->setChecked(true);

  connect(d->exact_match_radio, &QRadioButton::toggled, this,
          &EntityExplorer::QueryParametersChanged);

  d->containing_radio = new QRadioButton(tr("Containing"), this);
  query_mode_layout->addWidget(d->containing_radio);

  connect(d->containing_radio, &QRadioButton::toggled, this,
          &EntityExplorer::QueryParametersChanged);

  search_parameters_layout->addLayout(query_mode_layout);

  auto layout = new QVBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);

  auto category_combo_box = new CategoryComboBox(this);
  connect(category_combo_box, &CategoryComboBox::CategoryChanged, this,
          &EntityExplorer::OnCategoryChange);

  layout->addLayout(search_parameters_layout);
  layout->addWidget(category_combo_box);
  layout->addWidget(d->list_view);
  layout->addWidget(d->filter_widget);

  setContentsMargins(0, 0, 0, 0);
  setLayout(layout);

  connect(&ThemeManager::Get(), &ThemeManager::ThemeChanged, this,
          &EntityExplorer::OnThemeChange);

  OnThemeChange(ThemeManager::Get().GetPalette(),
                ThemeManager::Get().GetCodeViewTheme());
}

void EntityExplorer::InstallModel(IEntityExplorerModel *model,
                                  IGlobalHighlighter *global_highlighter) {
  d->model = model;

  QAbstractItemModel *source_model{d->model};
  if (global_highlighter != nullptr) {
    source_model = global_highlighter->CreateModelProxy(
        source_model, IEntityExplorerModel::TokenIdRole);
  }

  d->list_view->setModel(source_model);

  // Note: this needs to happen after the model has been set in the
  // list view!
  auto list_selection_model = d->list_view->selectionModel();

  connect(list_selection_model, &QItemSelectionModel::currentChanged, this,
          &EntityExplorer::SelectionChanged);

  connect(source_model, &QAbstractListModel::modelReset, this,
          &EntityExplorer::OnModelReset);

  OnModelReset();
}

void EntityExplorer::InstallItemDelegate(const CodeViewTheme &code_view_theme) {
  auto old_item_delegate = d->list_view->itemDelegate();
  if (old_item_delegate != nullptr) {
    old_item_delegate->deleteLater();
  }

  auto list_view_item_delegate =
      new EntityExplorerItemDelegate(code_view_theme, this);

  d->list_view->setItemDelegate(list_view_item_delegate);
}

//! Try to open the token related to a specific model index.
void EntityExplorer::SelectionChanged(const QModelIndex &index,
                                      const QModelIndex &) {
  if (!index.isValid()) {
    qDebug() << "invalid index";
    return;
  }

  QVariant token_var = index.data(IEntityExplorerModel::TokenRole);
  if (!token_var.isValid()) {
    qDebug() << "invalid tok";
    return;
  }

  emit EntityAction(qvariant_cast<Token>(token_var).id().Pack());
}

void EntityExplorer::OnModelReset() {}

void EntityExplorer::OnSearchParametersChange(
    const ISearchWidget::SearchParameters &search_parameters) {

  QRegularExpression::PatternOptions options{
      QRegularExpression::NoPatternOption};

  if (!search_parameters.case_sensitive) {
    options |= QRegularExpression::CaseInsensitiveOption;
  }

  auto pattern = QString::fromStdString(search_parameters.pattern);

  if (search_parameters.type == ISearchWidget::SearchParameters::Type::Text) {
    pattern = QRegularExpression::escape(pattern);
    if (search_parameters.whole_word) {
      pattern = "\\b" + pattern + "\\b";
    }
  }

  QRegularExpression regex(pattern, options);

  // The regex is already validated by the search widget
  Assert(regex.isValid(),
         "Invalid regex found in CodeView::OnSearchParametersChange");

  d->model->SetFilterRegularExpression(regex);
}

void EntityExplorer::QueryParametersChanged() {
  d->model->CancelSearch();

  if (d->search_input->text().isEmpty()) {
    return;
  }

  std::optional<IEntityExplorerModel::SearchMode> opt_search_mode;
  if (d->exact_match_radio->isChecked()) {
    opt_search_mode = IEntityExplorerModel::SearchMode::ExactMatch;

  } else if (d->containing_radio->isChecked()) {
    opt_search_mode = IEntityExplorerModel::SearchMode::Containing;
  }

  Assert(opt_search_mode.has_value(),
         "Invalid query mode state in the Entity Explorer widget");

  d->model->Search(d->search_input->text(), opt_search_mode.value());
}

void EntityExplorer::OnCategoryChange(
    const std::optional<TokenCategory> &opt_token_category) {
  if (!opt_token_category.has_value()) {
    d->model->SetTokenCategoryFilter(std::nullopt);
    return;
  }

  TokenCategory token_category = opt_token_category.value();

  IEntityExplorerModel::TokenCategorySet token_category_set;
  if (token_category == TokenCategory::UNKNOWN) {
    token_category_set = {
        TokenCategory::UNKNOWN,           TokenCategory::IDENTIFIER,
        TokenCategory::KEYWORD,           TokenCategory::OBJECTIVE_C_KEYWORD,
        TokenCategory::BUILTIN_TYPE_NAME, TokenCategory::PUNCTUATION,
        TokenCategory::LITERAL,           TokenCategory::COMMENT,
        TokenCategory::NAMESPACE,         TokenCategory::WHITESPACE,
        TokenCategory::FILE_NAME,         TokenCategory::LINE_NUMBER,
        TokenCategory::COLUMN_NUMBER,     TokenCategory::MACRO_PARAMETER_NAME,
        TokenCategory::LOCAL_VARIABLE,
    };
  } else {
    token_category_set = {token_category};
  }

  d->model->SetTokenCategoryFilter(token_category_set);
}

void EntityExplorer::OnThemeChange(const QPalette &,
                                   const CodeViewTheme &code_view_theme) {
  InstallItemDelegate(code_view_theme);

  QFont font{code_view_theme.font_name};
  setFont(font);
}

}  // namespace mx::gui
