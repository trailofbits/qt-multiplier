// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Explorers/EntityExplorer.h>

#include <QAction>
#include <QApplication>
#include <QBrush>
#include <QCheckBox>
#include <QListView>
#include <QMouseEvent>
#include <QPalette>
#include <QRadioButton>
#include <QSortFilterProxyModel>
#include <QTimer>
#include <QVBoxLayout>

#include <cctype>
#include <multiplier/AST/NamedDecl.h>
#include <multiplier/Frontend/File.h>
#include <multiplier/Frontend/DefineMacroDirective.h>
#include <multiplier/Frontend/TokenCategory.h>
#include <multiplier/Frontend/TokenKind.h>
#include <multiplier/GUI/Interfaces/IModel.h>
#include <multiplier/GUI/Managers/ActionManager.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/ThemeManager.h>
#include <multiplier/GUI/Widgets/LineEditWidget.h>
#include <multiplier/GUI/Widgets/ListGeneratorWidget.h>
#include <multiplier/Index.h>

#include "CategoryComboBox.h"

namespace mx::gui {
namespace {

class EntitySearchResult Q_DECL_FINAL : public IGeneratedItem {

  VariantEntity entity;
  RawEntityId aliased_entity_id;
  TokenRange name_tokens;

 public:
  virtual ~EntitySearchResult(void) = default;

  inline EntitySearchResult(VariantEntity entity_,
                            RawEntityId aliased_entity_id_,
                            TokenRange name_tokens_)
      : entity(std::move(entity_)),
        aliased_entity_id(aliased_entity_id_),
        name_tokens(std::move(name_tokens_)) {}

  VariantEntity Entity(void) const Q_DECL_FINAL {
    return entity;
  }

  // NOTE(pag): This must be non-blocking.
  RawEntityId AliasedEntityId(void) const Q_DECL_FINAL {
    return aliased_entity_id;
  }

  QVariant Data(int col) const Q_DECL_FINAL {
    if (col == 0) {
      return QVariant::fromValue(name_tokens);
    } else {
      return {};
    }
  }
};

class EntitySearchGenerator : public IListGenerator {
 private:
  const Index index;
  const std::string query;
  const bool exact;
  const std::optional<TokenCategory> category;
  std::vector<CustomToken> toks;

 public:
  virtual ~EntitySearchGenerator(void) = default;

  inline EntitySearchGenerator(Index index_, std::string query_, bool exact_,
                               std::optional<TokenCategory> category_)
      : index(std::move(index_)),
        query(std::move(query_)),
        exact(exact_),
        category(std::move(category_)) {}

  QString ColumnTitle(int) const Q_DECL_FINAL {
    return QObject::tr("Entity Name");
  }

  QString Name(const ITreeGeneratorPtr &) const Q_DECL_FINAL {
    return "";
  }

  gap::generator<IGeneratedItemPtr> Roots(
      const ITreeGeneratorPtr &) Q_DECL_FINAL;
};

// Generate the search results.
gap::generator<IGeneratedItemPtr> EntitySearchGenerator::Roots(
    const ITreeGeneratorPtr &) {
  
  if (query.empty()) {
    co_return;
  }

  for (NamedEntity result : index.query_entities(query)) {

    // It's a declaration.
    if (std::holds_alternative<NamedDecl>(result)) {
      NamedDecl decl = std::move(std::get<NamedDecl>(result));
      if (exact && decl.name() != query) {
        continue;
      }

      auto name = decl.token();
      if (category && name.category() != category.value()) {
        continue;
      }

      RawEntityId aliased_entity_id = decl.canonical_declaration().id().Pack();

      co_yield std::make_shared<EntitySearchResult>(
          std::move(decl), aliased_entity_id, std::move(name));

    // It's a macro.
    } else if (std::holds_alternative<DefineMacroDirective>(result)) {
      DefineMacroDirective macro
          = std::move(std::get<DefineMacroDirective>(result));
      if (exact && macro.name().data() != query) {
        continue;
      }

      auto name = macro.name();
      if (category && name.category() != category.value()) {
        continue;
      }

      co_yield std::make_shared<EntitySearchResult>(
          std::move(macro), kInvalidEntityId, std::move(name));
    
    // It's a file.
    } else if (std::holds_alternative<File>(result)) {

      if (category && TokenCategory::FILE_NAME != category.value()) {
        continue;
      }

      File file = std::move(std::get<File>(result));

      std::optional<std::string> matched_path;
      for (auto path : file.paths()) {
        auto path_str = path.generic_string();
        if (exact && query != path_str) {
          continue;
        }

        if (path_str.find(query) == std::string::npos) {
          continue;
        }

        UserToken tok;
        tok.kind = TokenKind::HEADER_NAME;
        tok.category = TokenCategory::FILE_NAME;
        tok.data = std::move(path_str);
        tok.related_entity = file;
        toks.emplace_back(std::move(tok));

        co_yield std::make_shared<EntitySearchResult>(
            std::move(file), kInvalidEntityId,
            TokenRange::create(std::move(toks)));
      }
    }
  }
}

}  // namespace

struct EntityExplorer::PrivateData {
  Index index;

  const ConfigManager &config_manager;

  QWidget *view{nullptr};
  ListGeneratorWidget *list_widget{nullptr};
  LineEditWidget *search_input{nullptr};
  QRadioButton *exact_match_radio{nullptr};
  QRadioButton *containing_radio{nullptr};

  std::optional<TokenCategory> category;

  QModelIndex context_index;
  QModelIndex clicked_index;

  // Action for opening an entity when the selection is changed.
  const TriggerHandle open_entity_trigger;

  inline PrivateData(ConfigManager &config_manager_)
      : config_manager(config_manager_),
        open_entity_trigger(config_manager.ActionManager().Find(
            "com.trailofbits.action.OpenEntity")) {}
};

EntityExplorer::~EntityExplorer(void) {}

EntityExplorer::EntityExplorer(ConfigManager &config_manager,
                               QMainWindow *parent)
    : IMainWindowPlugin(config_manager, parent),
      d(new PrivateData(config_manager)) {

  connect(&config_manager, &ConfigManager::IndexChanged,
          this, &EntityExplorer::OnIndexChanged);

  OnIndexChanged(d->config_manager);
}

QWidget *EntityExplorer::CreateDockWidget(QWidget *parent) {
  if (d->view) {
    return d->view;
  }

  auto &theme_manager = d->config_manager.ThemeManager();

  d->view = new QWidget(parent);
  d->view->setWindowTitle(tr("Entity Explorer"));

  auto search_parameters_layout = new QVBoxLayout();

  d->search_input = new LineEditWidget(d->view);
  d->search_input->setClearButtonEnabled(true);
  d->search_input->setPlaceholderText(tr("Search"));

  // Keep the font up-to-date.
  d->search_input->setFont(theme_manager.Theme()->Font());
  connect(&theme_manager, &ThemeManager::ThemeChanged,
          [this] (const ThemeManager &tm) {
            d->search_input->setFont(tm.Theme()->Font());
          });

  connect(d->search_input, &QLineEdit::textChanged,
          this, &EntityExplorer::QueryParametersChanged);

  search_parameters_layout->addWidget(d->search_input);

  auto query_mode_layout = new QHBoxLayout();
  d->exact_match_radio = new QRadioButton(tr("Exact Match"), d->view);

  d->containing_radio = new QRadioButton(tr("Containing"), d->view);
  d->containing_radio->setChecked(true);

  query_mode_layout->addWidget(d->exact_match_radio);
  query_mode_layout->addWidget(d->containing_radio);

  connect(d->exact_match_radio, &QRadioButton::toggled,
          this, &EntityExplorer::QueryParametersChanged);

  connect(d->containing_radio, &QRadioButton::toggled,
          this, &EntityExplorer::QueryParametersChanged);

  search_parameters_layout->addLayout(query_mode_layout);

  auto layout = new QVBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);

  auto category_combo_box = new CategoryComboBox(d->view);
  connect(category_combo_box, &CategoryComboBox::CategoryChanged,
          this, &EntityExplorer::OnCategoryChanged);

  d->list_widget = new ListGeneratorWidget(d->config_manager);
  connect(d->list_widget, &ListGeneratorWidget::RequestContextMenu,
          [this] (const QModelIndex &index) {
            d->context_index = index;
            emit RequestContextMenu(d->context_index);
          });

  connect(d->list_widget, &ListGeneratorWidget::SelectedItemChanged,
          [this] (const QModelIndex &index) {
            d->clicked_index = index;
            emit RequestPrimaryClick(d->clicked_index);
          });

  layout->addLayout(search_parameters_layout);
  layout->addWidget(category_combo_box);
  layout->addWidget(d->list_widget, 1);
  layout->addStretch();

  d->view->setContentsMargins(0, 0, 0, 0);
  d->view->setLayout(layout);

  return d->view;
}

void EntityExplorer::ActOnPrimaryClick(const QModelIndex &index) {
  if (!d->view || index != d->clicked_index || !index.isValid()) {
    return;
  }

  d->open_entity_trigger.Trigger(index.data(IModel::EntityRole));
}

void EntityExplorer::ActOnContextMenu(QMenu *menu, const QModelIndex &index) {
  if (!d->view || index != d->clicked_index || !index.isValid()) {
    return;
  }

  d->list_widget->ActOnContextMenu(menu, index);
}

void EntityExplorer::OnIndexChanged(const ConfigManager &config_manager) {
  d->index = config_manager.Index();
}

void EntityExplorer::QueryParametersChanged(void) {
  if (!d->list_widget) {
    return;
  }

  d->list_widget->InstallGenerator(std::make_shared<EntitySearchGenerator>(
      d->index, d->search_input->text().toStdString(),
      d->exact_match_radio->isChecked(), d->category));
}

void EntityExplorer::OnCategoryChanged(std::optional<TokenCategory> category) {
  d->category = std::move(category);
  QueryParametersChanged();
}

}  // namespace mx::gui
