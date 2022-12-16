// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "OmniBoxView.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFont>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSplitter>
#include <QString>
#include <QTableWidget>
#include <QTabWidget>
#include <QThreadPool>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QSizePolicy>

#include <cassert>
#include <iostream>
#include <sstream>
#include <multiplier/AST.h>
#include <multiplier/Index.h>
#include <multiplier/Re2.h>
#include <multiplier/Weggli.h>
#include <multiplier/CodeTheme.h>
#include <multiplier/Util.h>
#include <string>
#include <variant>

#include "CodeSearchResults.h"
#include "OldCodeView.h"
#include "Configuration.h"
#include "Multiplier.h"
#include "TitleNamePrompt.h"

namespace mx::gui {
namespace {

static QString EnumeratorToLabelName(const char *enumerator) {
  QString ret;
  auto uppercase = true;

  for (QChar ch : QString::fromUtf8(enumerator)) {
    if (ch == '_') {
      uppercase = true;
      ret.push_back(QChar::Space);

    } else if (uppercase) {
      ret.push_back(ch.toUpper());
      uppercase = false;

    } else {
      ret.push_back(ch.toLower());
    }
  }
  return ret;
}

enum : int {
  kKindColumnIndex,
  kNameColumnIndex,
  kPathColumnIndex,
  kFileColumnIndex,
  kLineColumnIndex,
  kColumnColumnIndex,

  kNumColumns
};

static bool IsSearchableCategory(DeclCategory c) {
  switch (c) {
    default:
    case DeclCategory::UNKNOWN:
    case DeclCategory::LOCAL_VARIABLE:
    case DeclCategory::THIS:
    case DeclCategory::LABEL:
    case DeclCategory::PARAMETER_VARIABLE:
    case DeclCategory::TEMPLATE_TYPE_PARAMETER:
    case DeclCategory::TEMPLATE_VALUE_PARAMETER:
      return false;
    case DeclCategory::GLOBAL_VARIABLE:
    case DeclCategory::FUNCTION:
    case DeclCategory::INSTANCE_METHOD:
    case DeclCategory::INSTANCE_MEMBER:
    case DeclCategory::CLASS_METHOD:
    case DeclCategory::CLASS_MEMBER:
    case DeclCategory::CLASS:
    case DeclCategory::STRUCTURE:
    case DeclCategory::UNION:
    case DeclCategory::INTERFACE:
    case DeclCategory::ENUMERATION:
    case DeclCategory::ENUMERATOR:
    case DeclCategory::NAMESPACE:
    case DeclCategory::TYPE_ALIAS:
      return true;
  }
  return false;
}

}  // namespace

struct OmniBoxView::PrivateData {
  Multiplier &multiplier;

  QVBoxLayout *layout{nullptr};
  QTabWidget *content{nullptr};

  QWidget *symbol_box{nullptr};
  QGridLayout *symbol_layout{nullptr};
  QLineEdit *symbol_input{nullptr};
  QPushButton *symbol_button{nullptr};
  QCheckBox *symbol_categories[NumEnumerators(DeclCategory{})];
  QTreeWidget *symbol_results{nullptr};
  QTreeWidgetItem *category_results[NumEnumerators(DeclCategory{})];
  std::unordered_map<QTreeWidgetItem *, std::pair<RawEntityId, NamedDecl>>
      item_to_entity;

  QWidget *entity_box{nullptr};
  QGridLayout *entity_layout{nullptr};
  OldCodeView *entity_result_code_view{nullptr};
  QLineEdit *entity_input{nullptr};
  QPushButton *entity_button{nullptr};
  QWidget *entity_results{nullptr};
  HighlightRangeTheme *entity_result_theme{nullptr};

  QWidget *regex_box{nullptr};
  QGridLayout *regex_layout{nullptr};
  QLineEdit *regex_input{nullptr};
  QPushButton *regex_button{nullptr};
  QPushButton *regex_to_tab_button{nullptr};
  QPushButton *regex_to_dock_button{nullptr};
  QWidget *regex_results{nullptr};
  RegexQuery regex_query;

  QWidget *weggli_box{nullptr};
  QGridLayout *weggli_layout{nullptr};
  QLineEdit *weggli_input{nullptr};
  QComboBox *weggli_lang{nullptr};
  QPushButton *weggli_button{nullptr};
  QPushButton *weggli_to_tab_button{nullptr};
  QPushButton *weggli_to_dock_button{nullptr};
  QWidget *weggli_results{nullptr};
  WeggliQuery weggli_query;

  unsigned symbol_counter{0};
  unsigned regex_counter{0};
  unsigned weggli_counter{0};
  unsigned entity_counter{0};

  std::unordered_map<RawEntityId, std::filesystem::path> file_id_to_path;

  inline PrivateData(Multiplier &multiplier_)
      : multiplier(multiplier_) {}
};

OmniBoxView::~OmniBoxView(void) {}

OmniBoxView::OmniBoxView(Multiplier &multiplier_, QWidget *parent_)
    : QWidget(parent_),
      d(std::make_unique<PrivateData>(multiplier_)) {
  InitializeWidgets();
}

void OmniBoxView::InitializeWidgets(void) {
  setWindowTitle(tr("Search"));

  d->layout = new QVBoxLayout;
  d->layout->setContentsMargins(0, 0, 0, 0);
  setLayout(d->layout);

  d->content = new QTabWidget;
  d->content->setDocumentMode(false);
  d->content->setTabBarAutoHide(false);
  d->layout->addWidget(d->content);

  // ---------------------------------------------------------------------------
  // Entity name search
  d->symbol_box = new QWidget;
  d->symbol_layout = new QGridLayout;
  d->symbol_input = new QLineEdit;
  d->symbol_button = new QPushButton(tr("Query"));
  for (auto category : EnumerationRange<DeclCategory>()) {
    auto c = static_cast<unsigned>(category);
    if (IsSearchableCategory(category)) {
      d->symbol_categories[c] = new QCheckBox;
      d->symbol_categories[c]->setChecked(true);

      // Possibly disable
      connect(d->symbol_categories[c], &QCheckBox::stateChanged,
              this, &OmniBoxView::MaybeDisableSymbolSearch);
    } else {
      d->symbol_categories[c] = nullptr;
    }
  }

  QFont input_font = d->multiplier.CodeTheme().Font();
  QFont button_font = d->symbol_button->font();
  button_font.setPointSize(input_font.pointSize());

  d->symbol_input->setFont(input_font);
  d->symbol_input->setFocus();
  d->symbol_button->setFont(button_font);
  d->symbol_button->setDisabled(true);

  auto num_categories = NumEnumerators(DeclCategory{});
  QFormLayout *column_layouts[2] = {new QFormLayout, new QFormLayout};

  auto columns_widget = new QWidget;
  auto columns_layout = new QHBoxLayout;
  columns_widget->setLayout(columns_layout);
  columns_layout->addLayout(column_layouts[0]);
  columns_layout->addLayout(column_layouts[1]);

  d->symbol_box->setLayout(d->symbol_layout);
  d->symbol_layout->addWidget(d->symbol_button, 0, 0, 1, 1, Qt::AlignTop);
  d->symbol_layout->addWidget(d->symbol_input, 0, 1, 1, 1, Qt::AlignTop);
  d->symbol_layout->addWidget(columns_widget, 1, 0, 1, 2, Qt::AlignTop);
  d->symbol_layout->setRowStretch(0, 0);
  d->symbol_layout->setRowStretch(1, 0);
  d->symbol_layout->setRowStretch(2, 1);

  for (auto i = 0u, j = 0u; i < num_categories; ++i) {
    auto c = static_cast<DeclCategory>(i);
    if (IsSearchableCategory(c)) {
      auto enumerator_name = EnumeratorName(c);
      column_layouts[j % 2]->addRow(
          EnumeratorToLabelName(enumerator_name) + ": ",
          d->symbol_categories[i]);
      ++j;
    }
  }

  d->content->addTab(d->symbol_box, tr("Symbol Search"));

  connect(d->symbol_input, &QLineEdit::textChanged,
          this, &OmniBoxView::SetSymbolQueryString);

  connect(d->symbol_input, &QLineEdit::returnPressed,
          this, &OmniBoxView::RunSymbolSearch);

  connect(d->symbol_button, &QPushButton::pressed,
          this, &OmniBoxView::RunSymbolSearch);

  // ---------------------------------------------------------------------------
  // Entity ID search
  d->entity_box = new QWidget;
  d->entity_layout = new QGridLayout;
  d->entity_input = new QLineEdit;
  d->entity_button = new QPushButton(tr("Query"));


  button_font.setPointSize(input_font.pointSize());

  d->entity_input->setFont(input_font);
  d->entity_input->setFocus();
  d->entity_button->setFont(button_font);
  d->entity_button->setDisabled(true);

  d->entity_box->setLayout(d->entity_layout);
  d->entity_layout->addWidget(d->entity_button, 0, 0, 1, 1, Qt::AlignTop);
  d->entity_layout->addWidget(d->entity_input, 0, 1, 1, 1, Qt::AlignTop);
  d->entity_layout->setRowStretch(0, 0);
  d->entity_layout->setRowStretch(1, 0);
  d->entity_layout->setRowStretch(2, 1);

  d->content->addTab(d->entity_box, tr("Go to Entity Id"));

  connect(d->entity_input, &QLineEdit::textChanged,
          this, &OmniBoxView::SetEntityIdQueryString);

  connect(d->entity_input, &QLineEdit::returnPressed,
          this, &OmniBoxView::RunEntityIdSearch);

  connect(d->entity_button, &QPushButton::pressed,
          this, &OmniBoxView::RunEntityIdSearch);

  connect(this, &OmniBoxView::EntityIdIsFile,
          &d->multiplier, &Multiplier::OnSourceFileDoubleClicked);

  // ---------------------------------------------------------------------------
  // Regex search
  d->regex_box = new QWidget;
  d->regex_layout = new QGridLayout;
  d->regex_input = new QLineEdit;
  d->regex_button = new QPushButton(tr("Query"));
  d->regex_to_tab_button = new QPushButton(tr("⍐ tab"));
  d->regex_to_dock_button = new QPushButton(tr("⍇ dock"));

  d->regex_input->setFont(input_font);
  d->regex_button->setFont(button_font);
  d->regex_to_tab_button->setFont(button_font);
  d->regex_to_dock_button->setFont(button_font);

  d->regex_box->setLayout(d->regex_layout);
  d->regex_layout->addWidget(d->regex_button, 0, 0, 1, 1, Qt::AlignTop);
  d->regex_layout->addWidget(d->regex_to_tab_button, 0, 2, 1, 1, Qt::AlignTop);
  d->regex_layout->addWidget(d->regex_to_dock_button, 0, 3, 1, 1, Qt::AlignTop);
  d->regex_layout->addWidget(d->regex_input, 0, 1, 1, 1, Qt::AlignTop);
  d->regex_layout->setRowStretch(0, 0);
  d->regex_layout->setRowStretch(1, 1);
  d->content->addTab(d->regex_box, tr("Regex Search"));
  d->regex_button->setDisabled(true);
  d->regex_to_dock_button->setDisabled(true);
  d->regex_to_tab_button->setDisabled(true);

  d->regex_layout->installEventFilter(&d->multiplier);

  connect(d->regex_input, &QLineEdit::textChanged,
          this, &OmniBoxView::BuildRegex);

  connect(d->regex_input, &QLineEdit::returnPressed,
          this, &OmniBoxView::RunRegex);

  connect(d->regex_button, &QPushButton::pressed,
          this, &OmniBoxView::RunRegex);

  connect(d->regex_to_dock_button, &QPushButton::pressed,
          this, &OmniBoxView::OnOpenRegexResultsInDock);

  connect(d->regex_to_tab_button, &QPushButton::pressed,
          this, &OmniBoxView::OnOpenRegexResultsInTab);

  connect(this, &OmniBoxView::OpenTab,
          &d->multiplier, &Multiplier::OnOpenTab);

  connect(this, &OmniBoxView::OpenDock,
          &d->multiplier, &Multiplier::OnOpenDock);

  // ---------------------------------------------------------------------------
  // Weggli search
  d->weggli_box = new QWidget;
  d->weggli_layout = new QGridLayout;
  d->weggli_input = new QLineEdit;
  d->weggli_lang = new QComboBox;
  d->weggli_button = new QPushButton(tr("Query"));
  d->weggli_to_tab_button = new QPushButton(tr("⍐ tab"));
  d->weggli_to_dock_button = new QPushButton(tr("⍇ dock"));

  d->weggli_input->setFont(input_font);
  d->weggli_button->setFont(button_font);
  d->weggli_lang->setFont(button_font);
  d->weggli_to_tab_button->setFont(button_font);
  d->weggli_to_dock_button->setFont(button_font);

  d->weggli_box->setLayout(d->weggli_layout);
  d->weggli_layout->addWidget(d->weggli_button, 0, 0, 1, 1, Qt::AlignTop);
  d->weggli_layout->addWidget(d->weggli_lang, 0, 2, 1, 1, Qt::AlignTop);
  d->weggli_layout->addWidget(d->weggli_to_tab_button, 0, 3, 1, 1, Qt::AlignTop);
  d->weggli_layout->addWidget(d->weggli_to_dock_button, 0, 4, 1, 1, Qt::AlignTop);
  d->weggli_layout->addWidget(d->weggli_input, 0, 1, 1, 1, Qt::AlignTop);
  d->weggli_layout->setRowStretch(0, 0);
  d->weggli_layout->setRowStretch(1, 1);
  d->content->addTab(d->weggli_box, tr("Weggli Search"));
  d->weggli_button->setDisabled(true);
  d->weggli_to_dock_button->setDisabled(true);
  d->weggli_to_tab_button->setDisabled(true);
  d->weggli_lang->addItem("C", QVariant(false));
  d->weggli_lang->addItem("C++", QVariant(true));
  d->weggli_lang->setDisabled(false);

  d->weggli_layout->installEventFilter(&d->multiplier);

  connect(d->weggli_input, &QLineEdit::textChanged,
          this, &OmniBoxView::BuildWeggli);

  connect(d->weggli_input, &QLineEdit::returnPressed,
          this, &OmniBoxView::RunWeggli);

  connect(d->weggli_button, &QPushButton::pressed,
          this, &OmniBoxView::RunWeggli);

  connect(d->weggli_to_dock_button, &QPushButton::pressed,
          this, &OmniBoxView::OnOpenWeggliResultsInDock);

  connect(d->weggli_to_tab_button, &QPushButton::pressed,
          this, &OmniBoxView::OnOpenWeggliResultsInTab);

  connect(this, &OmniBoxView::OpenTab,
          &d->multiplier, &Multiplier::OnOpenTab);

  connect(this, &OmniBoxView::OpenDock,
          &d->multiplier, &Multiplier::OnOpenDock);

  // ---------------------------------------------------------------------------
  // Generic

  d->content->hide();

  connect(this, &OmniBoxView::TokenPressEvent,
          &d->multiplier, &Multiplier::ActOnTokenPressEvent);
}

void OmniBoxView::Clear(void) {
  d->file_id_to_path.clear();

  d->regex_button->setDisabled(true);
  d->regex_input->setText(QString());
  d->regex_counter++;
  ClearRegexResults();

  d->weggli_button->setDisabled(true);
  d->weggli_input->setText(QString());
  d->weggli_counter++;
  ClearWeggliResults();

  d->symbol_button->setDisabled(true);
  d->symbol_input->setText(QString());
  d->symbol_counter++;
  ClearSymbolResults();

  d->entity_button->setDisabled(true);
  d->entity_input->setText(QString());
  d->entity_counter++;
  ClearEntityResults();
}

void OmniBoxView::ClearSymbolResults(void) {
  d->item_to_entity.clear();

  if (d->symbol_results) {
    d->symbol_layout->removeWidget(d->symbol_results);
    d->symbol_results->disconnect();
    d->symbol_results->deleteLater();
    d->symbol_results = nullptr;
    update();
  }
}

void OmniBoxView::ClearRegexResults(void) {
  if (d->regex_results) {
    d->regex_layout->removeWidget(d->regex_results);
    d->regex_results->disconnect();
    d->regex_results->deleteLater();
    d->regex_results = nullptr;
    d->regex_to_dock_button->setDisabled(true);
    d->regex_to_tab_button->setDisabled(true);
    update();
  }
}

void OmniBoxView::ClearWeggliResults(void) {
  if (d->weggli_results) {
    d->weggli_layout->removeWidget(d->weggli_results);
    d->weggli_results->disconnect();
    d->weggli_results->deleteLater();
    d->weggli_results = nullptr;
    d->weggli_to_dock_button->setDisabled(true);
    d->weggli_to_tab_button->setDisabled(true);
    update();
  }
}

void OmniBoxView::ClearEntityResults(void) {
  if (d->entity_results) {
    d->entity_layout->removeWidget(d->entity_results);
    d->entity_results->disconnect();
    d->entity_results->deleteLater();
    d->entity_results = nullptr;
    update();
  }
  if (d->entity_result_code_view) {
    d->entity_layout->removeWidget(d->entity_result_code_view);
    d->entity_result_code_view->Clear();
    d->entity_result_code_view->hide();
    d->entity_result_code_view->disconnect();
    d->entity_result_code_view->deleteLater();
    d->entity_result_code_view = nullptr;
    if (d->entity_result_theme) {
      delete d->entity_result_theme;
      d->entity_result_theme = nullptr;
    }
    update();
  }

}

void OmniBoxView::OpenWeggliSearch(void) {
  d->content->setCurrentWidget(d->weggli_box);
  d->weggli_input->setFocus();
}

void OmniBoxView::OpenRegexSearch(void) {
  d->content->setCurrentWidget(d->regex_box);
  d->regex_input->setFocus();
}

void OmniBoxView::OpenSymbolQuerySearch(void) {
  d->content->setCurrentWidget(d->symbol_box);
  d->symbol_input->setFocus();
}

void OmniBoxView::OpenEntitySearch(void) {
  d->content->setCurrentWidget(d->entity_box);
  d->entity_input->setFocus();
}

void OmniBoxView::Focus(void) {
  auto curr = d->content->currentWidget();
  if (curr == d->symbol_box) {
    d->symbol_input->setFocus();

  } else if (curr == d->regex_box) {
    d->regex_input->setFocus();
  }
}

void OmniBoxView::OnDownloadedFileList(FilePathList files) {
  for (auto &[path, index] : files) {
    d->file_id_to_path.emplace(index, std::move(path));
  }
}

void OmniBoxView::Disconnected(void) {
  d->content->hide();
  d->regex_counter++;
  d->symbol_counter++;
  d->file_id_to_path.clear();
  update();
}

void OmniBoxView::Connected(void) {
  d->content->show();
  update();
}

void OmniBoxView::MaybeDisableSymbolSearch(int) {
  auto text = d->symbol_input->text();
  if (0 >= text.size()) {
    d->symbol_button->setDisabled(true);
    return;
  }

  // Make sure at least one declaration kind is checked.
  for (auto category : EnumerationRange<DeclCategory>()) {
    auto box = d->symbol_categories[static_cast<unsigned>(category)];
    if (box && box->isChecked()) {
      d->symbol_button->setEnabled(true);
      return;
    }
  }

  d->symbol_button->setDisabled(true);
}

void OmniBoxView::SetSymbolQueryString(const QString &) {
  MaybeDisableSymbolSearch(0);
}

void OmniBoxView::RunSymbolSearch(void) {
  QString query = d->symbol_input->text();
  if (0 >= query.size()) {
    return;
  }

  d->symbol_counter++;
  ClearSymbolResults();

  d->symbol_results = new QTreeWidget;
  d->symbol_results->setColumnCount(kNumColumns);
  d->symbol_results->setSortingEnabled(true);
  d->symbol_results->setSelectionMode(
      QAbstractItemView::SelectionMode::SingleSelection);
  d->symbol_results->setHeaderHidden(false);
  d->symbol_results->setAutoScroll(false);
  d->symbol_results->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  d->symbol_results->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

  QTreeWidgetItem *header_item = d->symbol_results->headerItem();
  header_item->setText(kKindColumnIndex, tr("Declaration Kind"));
  header_item->setText(kNameColumnIndex, tr("Symbol Name"));
  header_item->setText(kPathColumnIndex, tr("Path"));
  header_item->setText(kFileColumnIndex, tr("File"));
  header_item->setText(kLineColumnIndex, tr("Line"));
  header_item->setText(kColumnColumnIndex, tr("Column"));

  // Customize visibility of columns.
  EntitySearchResultsConfiguration &config =
      d->multiplier.Configuration().entity_search_results;
  d->symbol_results->setColumnHidden(kKindColumnIndex,
                                     !config.show_declaration_kind);
  d->symbol_results->setColumnHidden(kPathColumnIndex, !config.show_file_path);
  d->symbol_results->setColumnHidden(kFileColumnIndex, !config.show_file_name);
  d->symbol_results->setColumnHidden(kLineColumnIndex,
                                     !config.show_line_numbers);
  d->symbol_results->setColumnHidden(kColumnColumnIndex,
                                     !config.show_column_numbers);

  QHeaderView *header = d->symbol_results->header();
  header->setStretchLastSection(true);
  header->setSectionResizeMode(QHeaderView::ResizeToContents);

  d->symbol_layout->addWidget(d->symbol_results, 2, 0, 1, 2);

  // Make sure at least one declaration kind is checked.
  for (auto category : EnumerationRange<DeclCategory>()) {
    auto c = static_cast<unsigned>(category);
    auto box = d->symbol_categories[c];
    if (!box || !box->isChecked()) {
      d->category_results[c] = nullptr;
      continue;
    }

    d->category_results[c] = new QTreeWidgetItem;
    d->category_results[c]->setText(
        0, EnumeratorToLabelName(EnumeratorName(category)) + "s");
    d->symbol_results->addTopLevelItem(d->category_results[c]);

    auto loading_child = new QTreeWidgetItem;
    loading_child->setText(0, tr("Querying..."));
    d->category_results[c]->addChild(loading_child);

    auto runnable = new SymbolSearchThread(
        d->multiplier.Index(), d->multiplier.FileLocationCache(),
        query, category, d->symbol_counter);
    runnable->setAutoDelete(true);

    connect(runnable, &SymbolSearchThread::FoundSymbols,
            this, &OmniBoxView::OnFoundSymbols);

    QThreadPool::globalInstance()->start(runnable);
  }

  d->symbol_results->viewport()->installEventFilter(&d->multiplier);

  connect(d->symbol_results, &QTreeWidget::itemPressed,
          this, &OmniBoxView::OnSymbolItemClicked);

  connect(d->symbol_results, &QTreeWidget::itemDoubleClicked,
          this, &OmniBoxView::OnSymbolItemClicked);

  d->symbol_results->expandAll();
  update();
}

void OmniBoxView::OnSymbolItemClicked(QTreeWidgetItem *item, int column) {
  auto it = d->item_to_entity.find(item);
  if (it == d->item_to_entity.end()) {
    return;
  }

  const auto &[tok_id, decl] = it->second;
  EventLocation loc;
  loc.SetReferencedDeclarationId(decl.id());
  if (auto frag_tok = decl.token()) {
    loc.SetParsedTokenId(frag_tok.id());
  }
  loc.SetFileTokenId(tok_id);

  emit TokenPressEvent(EventSource::kEntitySearchResult, loc);
}

void OmniBoxView::FillRow(QTreeWidgetItem *item, const NamedDecl &decl) const {
  QFont code_font = d->multiplier.CodeTheme().Font();
  code_font.setPointSizeF(item->font(0).pointSizeF());

  auto color = qApp->palette().text().color();
  color = QColor::fromRgbF(
      color.redF(), color.greenF(), color.blueF(), color.alphaF() * 0.75);

  EntitySearchResultsConfiguration &config =
      d->multiplier.Configuration().entity_search_results;

  std::string_view name = decl.name();

  item->setText(kKindColumnIndex,
                EnumeratorToLabelName(EnumeratorName(decl.kind())));
  item->setText(kNameColumnIndex, QString::fromUtf8(
      name.data(), static_cast<int>(name.size())));
  item->setFont(kNameColumnIndex, code_font);
  auto nearest_tok = DeclFileToken(decl);
  if (!nearest_tok) {
    return;
  }

  Token tok = nearest_tok.value();
  d->item_to_entity.try_emplace(item, tok.id(), decl);
  auto loc = tok.nearest_location(d->multiplier.FileLocationCache());
  if (!loc) {
    return;
  }

  // Show the line and column numbers.
  auto file = File::containing(tok);
  RawEntityId file_id = file ? file->id().Pack() : kInvalidEntityId;

  if (auto fp_it = d->file_id_to_path.find(file_id);
      fp_it != d->file_id_to_path.end()) {
    item->setForeground(kPathColumnIndex, color);
    item->setTextAlignment(kPathColumnIndex, Qt::AlignRight);
    item->setText(
        kPathColumnIndex,
        QString::fromStdString(fp_it->second.generic_string()));
    assert(!item->text(kPathColumnIndex).startsWith(QChar::Space));

    item->setForeground(kFileColumnIndex, color);
    item->setTextAlignment(kFileColumnIndex, Qt::AlignRight);
    item->setText(
        kFileColumnIndex,
        QString::fromStdString(fp_it->second.filename().string()));

#ifndef QT_NO_TOOLTIP
    item->setToolTip(
        kFileColumnIndex,
        QString::fromStdString(fp_it->second.generic_string()));
#endif
  }

  item->setForeground(kLineColumnIndex, color);
  item->setText(kLineColumnIndex, QString::number(loc->first));  // Line.
#ifndef QT_NO_TOOLTIP
  item->setToolTip(kLineColumnIndex, tr("Line %1").arg(loc->first));
#endif

  item->setForeground(kColumnColumnIndex, color);
  item->setText(kColumnColumnIndex, QString::number(loc->second));  // Column.
#ifndef QT_NO_TOOLTIP
  item->setToolTip(kColumnColumnIndex, tr("Column %1").arg(loc->second));
#endif

}

void OmniBoxView::OnFoundSymbols(NamedDeclList symbols, DeclCategory category,
                                 unsigned counter) {
  if (counter != d->symbol_counter) {
    return;  // Query returned too late.
  }

  auto c = static_cast<unsigned>(category);
  auto parent_item = d->category_results[c];

  // Remove the `Querying...` item.
  while (parent_item->childCount()) {
    parent_item->removeChild(parent_item->child(0));
  }

  if (symbols.empty()) {
    auto loading_child = new QTreeWidgetItem;
    loading_child->setText(0, tr("No results"));
    d->category_results[c]->addChild(loading_child);
    return;
  }

  for (const NamedDecl &decl : symbols) {
    auto decl_child = new QTreeWidgetItem;
    FillRow(decl_child, decl);
    d->category_results[c]->addChild(decl_child);
  }
}

void OmniBoxView::SetEntityIdQueryString(const QString &text) {
  if (text.isEmpty()) {
    d->entity_button->setDisabled(true);
  } else if (text.toULong()) {
    d->entity_button->setDisabled(false);
  } else {
    d->entity_button->setDisabled(true);
  }
  ClearEntityResults();
}

void OmniBoxView::RunEntityIdSearch(void) {
  RawEntityId entity_id;

  std::istringstream iss(d->entity_input->text().toStdString().c_str());

  iss >> entity_id;

  d->entity_counter++;

  ClearEntityResults();
  d->entity_results = new QLabel(tr("Loading entity"));
  d->entity_layout->addWidget(d->entity_results, 1, 0, 1, 4,
                             Qt::AlignmentFlag::AlignHCenter |
                             Qt::AlignmentFlag::AlignVCenter);

  auto runnable = new EntitySearchThread(
      d->multiplier.Index(), d->multiplier.FileLocationCache(),
      entity_id, d->entity_counter);
  runnable->setAutoDelete(true);

  connect(runnable, &EntitySearchThread::FoundEntity,
      this, &OmniBoxView::OnFoundEntity);

  QThreadPool::globalInstance()->start(runnable);

}

void OmniBoxView::OnFoundEntity(VariantEntity maybe_entity, unsigned counter) {
  if (d->entity_counter != counter) {
    return;
  }

  ClearEntityResults();

  std::optional<Fragment> frag;
  std::optional<File> file;
  std::optional<std::variant<TokenRange, MacroSubstitution>> highlight_tok;


  if (std::holds_alternative<Decl>(maybe_entity)) {
    auto entity = std::get<Decl>(maybe_entity);
    frag.emplace(Fragment::containing(entity));
    file.emplace(File::containing(entity));
    if (auto tok = DeclFileToken(entity)) {
      highlight_tok.emplace(*tok);
    } else {
      highlight_tok.emplace(entity.tokens());
    }

  } else if (std::holds_alternative<Stmt>(maybe_entity)) {
    auto entity = std::get<Stmt>(maybe_entity);
    frag.emplace(Fragment::containing(entity));
    file.emplace(File::containing(entity));
    highlight_tok.emplace(entity.tokens());

  } else if (std::holds_alternative<Token>(maybe_entity)) {
    auto entity = std::get<Token>(maybe_entity);
    if (Fragment::containing(entity)) {
      frag.emplace(Fragment::containing(entity).value());
    }
    if (File::containing(entity)) {
      file.emplace(File::containing(entity).value());
    }
    highlight_tok.emplace(entity);

  } else if (std::holds_alternative<Fragment>(maybe_entity)) {
    auto entity = std::get<Fragment>(maybe_entity);
    frag.emplace(entity);
    file.emplace(File::containing(entity));
    highlight_tok.emplace(entity.file_tokens());

  } else if (std::holds_alternative<Type>(maybe_entity)) {
    auto entity = std::get<Type>(maybe_entity);
    frag.emplace(Fragment::containing(entity));
    file.emplace(File::containing(entity));
    highlight_tok.emplace(frag->file_tokens());

  } else if (std::holds_alternative<Attr>(maybe_entity)) {
    auto entity = std::get<Attr>(maybe_entity);
    frag.emplace(Fragment::containing(entity));
    file.emplace(File::containing(*frag));
    highlight_tok.emplace(entity.tokens());

  } else if (std::holds_alternative<MacroSubstitution>(maybe_entity)) {
    auto entity = std::get<MacroSubstitution>(maybe_entity);
    frag.emplace(Fragment::containing(entity));
    file.emplace(File::containing(entity));

  } else if (std::holds_alternative<Designator>(maybe_entity)) {
    auto entity = std::get<Designator>(maybe_entity);
    frag.emplace(Fragment::containing(entity));
    file.emplace(File::containing(entity));
    highlight_tok.emplace(entity.tokens());

  } else if (std::holds_alternative<File>(maybe_entity)) {
    auto entity = std::get<File>(maybe_entity);

    if (!d->file_id_to_path.contains(entity.id())) {
      d->entity_results = new QLabel(tr("Filepath for ID not found"));
      d->entity_layout->addWidget(d->entity_results, 1, 0, 1, 4,
                                 Qt::AlignmentFlag::AlignHCenter |
                                 Qt::AlignmentFlag::AlignVCenter);
      update();
      return;

    }

    emit EntityIdIsFile(d->file_id_to_path[entity.id()], entity.id());
    return;

  }

  if (!frag && !file) {
    d->entity_results = new QLabel(tr("No matches"));
    d->entity_layout->addWidget(d->entity_results, 1, 0, 1, 4,
                               Qt::AlignmentFlag::AlignHCenter |
                               Qt::AlignmentFlag::AlignVCenter);
    update();
    return;
  }

  if (highlight_tok) {
    d->entity_result_theme = new HighlightRangeTheme(d->multiplier.CodeTheme());
    d->entity_result_code_view = new OldCodeView(*d->entity_result_theme,
        d->multiplier.FileLocationCache(), d->multiplier.Index());
  }

  connect(d->entity_result_code_view, &OldCodeView::SetSingleEntityGlobal,
          &d->multiplier, &Multiplier::SetSingleEntityGlobal);

  connect(d->entity_result_code_view, &OldCodeView::SetMultipleEntitiesGlobal,
          &d->multiplier, &Multiplier::SetMultipleEntitiesGlobal);

  d->multiplier.CodeTheme().BeginTokens();

  d->entity_layout->addWidget(d->entity_result_code_view, 1, 0,
      d->entity_layout->rowCount() - 1, d->entity_layout->columnCount());
  d->entity_result_code_view->viewport()->installEventFilter(&(d->multiplier));
  d->entity_result_code_view->show();

  if (d->entity_result_theme && highlight_tok) {
    auto tok = std::get<TokenRange>(*highlight_tok);
    if (std::holds_alternative<Type>(maybe_entity)) {
      d->entity_result_theme->HighlightTypeInFileTokenRange(tok, std::get<Type>(maybe_entity));

    } else {
      d->entity_result_theme->HighlightFileTokenRange(tok);
    }
  }

  // If the entity to show is a file or fragment, then show the whole file,
  // so that the 'context' is the file, but the location is the fragment within
  // the file. Otherwise, the context is a fragment, and the location is something
  // inside of the fragment. 
  if ((std::holds_alternative<Fragment>(maybe_entity) || !frag) && file) {
    d->entity_result_code_view->SetFile(*file);

  } else {
    d->entity_result_code_view->SetFragment(*frag);
  }
  
  if (highlight_tok && std::holds_alternative<mx::TokenRange>(*highlight_tok)) {

    // TODO(pag): Use macro substitutions.
    d->entity_result_code_view->ScrollToFileToken(
        std::get<mx::TokenRange>(*highlight_tok).file_tokens());

  } else if (frag) {
    d->entity_result_code_view->ScrollToFileToken(frag->file_tokens());
  } else if (auto entity = std::get<Token>(maybe_entity)) {
    d->entity_result_code_view->ScrollToFileToken(entity);
  }

  // Event to reference view of a file at selected token (cmd + click)
  connect(d->entity_result_code_view, &OldCodeView::TokenPressEvent,
      this, &OmniBoxView::OnEntityTokenPressEvent);

  d->multiplier.CodeTheme().EndTokens();
  update();

}

void OmniBoxView::OnEntityTokenPressEvent(EventLocations locs) {
  for (EventLocation loc : locs) {
    emit TokenPressEvent(EventSource::kEntityIDSearchResultSource, loc);
    if (loc.UnpackDeclarationId()) {
      loc.SetParsedTokenId(kInvalidEntityId);
      loc.SetFileTokenId(kInvalidEntityId);
      emit TokenPressEvent(EventSource::kEntityIDSearchResultDest, loc);
    }
  }
}

void OmniBoxView::BuildRegex(const QString &text) {
  if (text.isEmpty()) {
    d->regex_query = RegexQuery();
    d->regex_button->setDisabled(true);

  } else {
    auto arr = text.toUtf8();
    d->regex_query = RegexQuery(
        std::string(arr.data(), static_cast<size_t>(arr.size())));
    d->regex_button->setDisabled(!d->regex_query.IsValid());
  }
}

void OmniBoxView::RunRegex(void) {
  if (!d->regex_query.IsValid()) {
    return;
  }

  d->regex_counter++;

  ClearRegexResults();
  d->regex_results = new QLabel(tr("Querying..."));
  d->regex_layout->addWidget(d->regex_results, 1, 0, 1, 4,
                             Qt::AlignmentFlag::AlignHCenter |
                             Qt::AlignmentFlag::AlignVCenter);

  auto runnable = new RegexQueryThread(
      d->multiplier.Index(), d->regex_query, d->regex_counter);
  runnable->setAutoDelete(true);

  connect(runnable, &RegexQueryThread::FoundFragments,
          this, &OmniBoxView::OnFoundFragmentsWithRegex);

  QThreadPool::globalInstance()->start(runnable);
}

void OmniBoxView::OnFoundFragmentsWithRegex(RegexQueryResultIterator *list_,
                                            unsigned counter) {
  std::unique_ptr<RegexQueryResultIterator> list(list_);
  if (d->regex_counter != counter) {
    return;
  }

  ClearRegexResults();
  const CodeTheme &theme = d->multiplier.CodeTheme();
  theme.BeginTokens();
  CodeSearchResultsModel * model;

  for (auto j = 1; *list != IteratorEnd{}; ++*list, ++j) {
    const RegexQueryMatch &match = **list;
    if (!d->regex_results) {
      model = new CodeSearchResultsModel(d->multiplier);
      auto table = new CodeSearchResultsView(model);

      connect(table, &CodeSearchResultsView::TokenPressEvent,
              &d->multiplier, &Multiplier::ActOnTokenPressEvent);

      d->regex_results = table;
      d->regex_to_dock_button->setEnabled(true);
      d->regex_to_tab_button->setEnabled(true);
      d->regex_layout->addWidget(d->regex_results, 1, 0, 1, 4);
    }
    model->AddResult(match);
  }

  theme.EndTokens();

  if (!d->regex_results) {
    d->regex_results = new QLabel(tr("No matches"));
    d->regex_layout->addWidget(d->regex_results, 1, 0, 1, 4,
                               Qt::AlignmentFlag::AlignHCenter |
                               Qt::AlignmentFlag::AlignVCenter);
  }
  update();
}

void OmniBoxView::OnOpenRegexResultsInTab(void) {
  if (!d->regex_results) {
    return;
  }

  TitleNamePrompt dialog(tr("Set tab name"), this);
  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  d->regex_layout->removeWidget(d->regex_results);
  d->regex_results->setParent(nullptr);
  emit OpenTab(dialog.NewName(), d->regex_results);
  d->regex_results = nullptr;
  d->regex_to_dock_button->setDisabled(true);
  d->regex_to_tab_button->setDisabled(true);
  update();
}

void OmniBoxView::OnOpenRegexResultsInDock(void) {
  if (!d->regex_results) {
    return;
  }

  TitleNamePrompt dialog(tr("Set dock name"), this);
  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  d->regex_layout->removeWidget(d->regex_results);
  d->regex_results->setParent(nullptr);
  emit OpenDock(dialog.NewName(), d->regex_results);
  d->regex_results = nullptr;
  d->regex_to_dock_button->setDisabled(true);
  d->regex_to_tab_button->setDisabled(true);
  update();
}

void OmniBoxView::BuildWeggli(const QString &text) {
  if (text.isEmpty()) {
    d->weggli_query = WeggliQuery();
    d->weggli_button->setDisabled(true);

  } else {
    auto arr = text.toUtf8();
    d->weggli_query = WeggliQuery(
        std::string(arr.data(), static_cast<size_t>(arr.size())),
        d->weggli_lang->itemData(
            d->weggli_lang->currentIndex(), Qt::UserRole).toBool());
    d->weggli_button->setDisabled(!d->weggli_query.IsValid());
  }
}

void OmniBoxView::RunWeggli(void) {
  if (!d->weggli_query.IsValid()) {
    return;
  }

  d->weggli_counter++;

  ClearWeggliResults();
  d->weggli_results = new QLabel(tr("Querying..."));
  d->weggli_layout->addWidget(d->weggli_results, 1, 0, 1, 5,
                              Qt::AlignmentFlag::AlignHCenter |
                              Qt::AlignmentFlag::AlignVCenter);

  auto runnable = new WeggliQueryThread(
      d->multiplier.Index(), d->weggli_query, d->weggli_counter);
  runnable->setAutoDelete(true);

  connect(runnable, &WeggliQueryThread::FoundFragments,
          this, &OmniBoxView::OnFoundFragmentsWithWeggli);

  QThreadPool::globalInstance()->start(runnable);
}

void OmniBoxView::OnFoundFragmentsWithWeggli(WeggliQueryResultIterator *list_,
                                            unsigned counter) {
  std::unique_ptr<WeggliQueryResultIterator> list(list_);
  if (d->weggli_counter != counter) {
    return;
  }

  ClearWeggliResults();

  const CodeTheme &theme = d->multiplier.CodeTheme();
  theme.BeginTokens();

  auto model = new CodeSearchResultsModel(d->multiplier);
  auto table = new CodeSearchResultsView(model);

  connect(table, &CodeSearchResultsView::TokenPressEvent,
          &d->multiplier, &Multiplier::ActOnTokenPressEvent);

  for (auto j = 1; *list != IteratorEnd{}; ++*list, ++j) {
    const WeggliQueryMatch &match = **list;
    if (!d->weggli_results) {
      d->weggli_results = table;
      d->weggli_to_dock_button->setEnabled(true);
      d->weggli_to_tab_button->setEnabled(true);
      d->weggli_layout->addWidget(d->weggli_results, 1, 0, 1, 5);
    }
    model->AddResult(match);
  }

  theme.EndTokens();

  if (!d->weggli_results) {
    d->weggli_results = new QLabel(tr("No matches"));
    d->weggli_layout->addWidget(d->weggli_results, 1, 0, 1, 5,
                               Qt::AlignmentFlag::AlignHCenter |
                               Qt::AlignmentFlag::AlignVCenter);
  }
  update();
}

void OmniBoxView::OnOpenWeggliResultsInTab(void) {
  if (!d->weggli_results) {
    return;
  }

  TitleNamePrompt dialog(tr("Set tab name"), this);
  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  d->weggli_layout->removeWidget(d->weggli_results);
  d->weggli_results->setParent(nullptr);
  emit OpenTab(dialog.NewName(), d->weggli_results);
  d->weggli_results = nullptr;
  d->weggli_to_dock_button->setDisabled(true);
  d->weggli_to_tab_button->setDisabled(true);
  update();
}

void OmniBoxView::OnOpenWeggliResultsInDock(void) {
  if (!d->weggli_results) {
    return;
  }

  TitleNamePrompt dialog(tr("Set dock name"), this);
  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  d->weggli_layout->removeWidget(d->weggli_results);
  d->weggli_results->setParent(nullptr);
  emit OpenDock(dialog.NewName(), d->weggli_results);
  d->weggli_results = nullptr;
  d->weggli_to_dock_button->setDisabled(true);
  d->weggli_to_tab_button->setDisabled(true);
  update();
}

struct SymbolSearchThread::PrivateData {
  const Index index;
  const FileLocationCache &file_cache;
  const QString query;
  const DeclCategory category;
  const unsigned counter;

  inline PrivateData(const Index &index_, const FileLocationCache &cache_,
                     const QString &query_,
                     DeclCategory category_, unsigned counter_)
      : index(index_),
        file_cache(cache_),
        query(query_),
        category(category_),
        counter(counter_) {}
};

SymbolSearchThread::~SymbolSearchThread(void) {}

SymbolSearchThread::SymbolSearchThread(
    const Index &index_, const FileLocationCache &cache_,
    const QString &query_, DeclCategory category_, unsigned counter_)
    : d(std::make_unique<PrivateData>(
          index_, cache_, query_, category_, counter_)) {}

void SymbolSearchThread::run(void) {
  NamedDeclList decls = d->index.query_entities(
      d->query.toStdString(), d->category);

  for (const NamedDecl &decl : decls) {
    d->file_cache.add(File::containing(decl));
  }

  emit FoundSymbols(std::move(decls), d->category, d->counter);
}

struct EntitySearchThread::PrivateData {
  const Index index;
  const FileLocationCache &file_cache;
  const RawEntityId raw_id;
  const unsigned counter;

  inline PrivateData(const Index &index_,
                     const FileLocationCache &cache_,
                     const RawEntityId raw_id_, unsigned counter_)
      : index(index_),
        file_cache(cache_),
        raw_id(raw_id_),
        counter(counter_) {}
};

EntitySearchThread::~EntitySearchThread(void) {}

EntitySearchThread::EntitySearchThread(const Index &index_,
                                       const FileLocationCache &cache_,
                                       const RawEntityId raw_id_,
                                       unsigned counter_)
    : d(std::make_unique < PrivateData > (index_, cache_, raw_id_, counter_)) {}

void EntitySearchThread::run(void) {
  emit FoundEntity(d->index.entity(d->raw_id), d->counter);
}

struct RegexQueryThread::PrivateData {
  const Index index;
  const RegexQuery query;
  const unsigned counter;

  inline PrivateData(const Index &index_, const RegexQuery &query_,
                     unsigned counter_)
      : index(index_),
        query(query_),
        counter(counter_) {}
};

RegexQueryThread::~RegexQueryThread(void) {}

RegexQueryThread::RegexQueryThread(const Index &index_,
                                   const RegexQuery &query_,
                                   unsigned counter_)
    : d(std::make_unique<PrivateData>(index_, query_, counter_)) {}

void RegexQueryThread::run(void) {
  emit FoundFragments(
      new RegexQueryResultIterator(d->index.query_fragments(d->query).begin()),
      d->counter);
}

struct WeggliQueryThread::PrivateData {
  const Index index;
  const WeggliQuery query;
  const unsigned counter;

  inline PrivateData(const Index &index_, const WeggliQuery &query_,
                     unsigned counter_)
      : index(index_),
        query(query_),
        counter(counter_) {}
};

WeggliQueryThread::~WeggliQueryThread(void) {}

WeggliQueryThread::WeggliQueryThread(const Index &index_,
                                     const WeggliQuery &query_,
                                     unsigned counter_)
    : d(std::make_unique<PrivateData>(index_, query_, counter_)) {}

void WeggliQueryThread::run(void) {
  emit FoundFragments(
      new WeggliQueryResultIterator(d->index.query_fragments(d->query).begin()),
      d->counter);
}

}  // namespace mx::gui
