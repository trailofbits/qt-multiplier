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
#include <QKeySequence>
#include <QListView>
#include <QMenu>
#include <QMouseEvent>
#include <QPalette>
#include <QRadioButton>
#include <QShortcut>
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
#include <multiplier/GUI/Interfaces/IWindowManager.h>
#include <multiplier/GUI/Interfaces/IWindowWidget.h>
#include <multiplier/GUI/Managers/ActionManager.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/ThemeManager.h>
#include <multiplier/GUI/Widgets/LineEditWidget.h>
#include <multiplier/GUI/Widgets/ListGeneratorWidget.h>
#include <multiplier/GUI/Util.h>
#include <multiplier/Index.h>

#include "CategoryComboBox.h"

namespace mx::gui {
namespace {

static const QString kModelId = "com.trailofbits.explorer.EntityExplorer.EntityListModel";

static const QKeySequence kKeqSeqCtrlShiftF("Ctrl+Shift+F");

class EntitySearchResult Q_DECL_FINAL : public IGeneratedItem {

  VariantEntity entity;
  VariantEntity aliased_entity;
  TokenRange name_tokens;

 public:
  virtual ~EntitySearchResult(void) = default;

  inline EntitySearchResult(VariantEntity entity_,
                            VariantEntity aliased_entity_,
                            TokenRange name_tokens_)
      : entity(std::move(entity_)),
        aliased_entity(aliased_entity_),
        name_tokens(std::move(name_tokens_)) {}

  VariantEntity Entity(void) const Q_DECL_FINAL {
    return entity;
  }

  VariantEntity AliasedEntity(void) const Q_DECL_FINAL {
    return aliased_entity;
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

  gap::generator<IGeneratedItemPtr> Roots(ITreeGeneratorPtr) Q_DECL_FINAL;
};

// Generate the search results.
gap::generator<IGeneratedItemPtr> EntitySearchGenerator::Roots(
    ITreeGeneratorPtr) {
  
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

      co_yield std::make_shared<EntitySearchResult>(
          std::move(decl), decl.canonical_declaration(),
          NameOfEntity(decl));

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
          std::move(macro), NotAnEntity{}, std::move(name));
    
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
            std::move(file), NotAnEntity{},
            TokenRange::create(std::move(toks)));
      }
    }
  }
}

}  // namespace

struct EntityExplorer::PrivateData {
  Index index;

  const ConfigManager &config_manager;

  IWindowWidget *view{nullptr};
  ListGeneratorWidget *list_widget{nullptr};
  LineEditWidget *search_input{nullptr};
  QRadioButton *exact_match_radio{nullptr};
  QRadioButton *containing_radio{nullptr};
  QShortcut *shortcut{nullptr};

  std::optional<TokenCategory> category;

  // Action for opening an entity when the selection is changed.
  const TriggerHandle open_entity_trigger;

  inline PrivateData(ConfigManager &config_manager_)
      : config_manager(config_manager_),
        open_entity_trigger(config_manager.ActionManager().Find(
            "com.trailofbits.action.OpenEntity")) {}
};

EntityExplorer::~EntityExplorer(void) {}

EntityExplorer::EntityExplorer(ConfigManager &config_manager,
                               IWindowManager *parent)
    : IMainWindowPlugin(config_manager, parent),
      d(new PrivateData(config_manager)) {

  connect(&config_manager, &ConfigManager::IndexChanged,
          this, &EntityExplorer::OnIndexChanged);

  OnIndexChanged(d->config_manager);
  CreateDockWidget(parent);
}

void EntityExplorer::CreateDockWidget(IWindowManager *manager) {
  auto &theme_manager = d->config_manager.ThemeManager();

  d->view = new IWindowWidget;
  d->view->setWindowTitle(tr("Entity Explorer"));

  auto search_parameters_layout = new QVBoxLayout;

  d->search_input = new LineEditWidget(d->view);
  d->search_input->setClearButtonEnabled(true);
  d->search_input->setPlaceholderText(
      tr("Search (%1)").arg(
          kKeqSeqCtrlShiftF.toString(QKeySequence::NativeText)));

  // Keep the font up-to-date.
  d->search_input->setFont(theme_manager.Theme()->Font());
  connect(&theme_manager, &ThemeManager::ThemeChanged,
          d->search_input, [this] (const ThemeManager &tm) {
                             d->search_input->setFont(tm.Theme()->Font());
                           });

  connect(d->search_input, &QLineEdit::textChanged,
          this, &EntityExplorer::QueryParametersChanged);

  search_parameters_layout->addWidget(d->search_input);

  auto query_mode_layout = new QHBoxLayout;
  d->exact_match_radio = new QRadioButton(tr("Exact Match"), d->view);

  d->containing_radio = new QRadioButton(tr("Word Prefix Match"), d->view);
  d->containing_radio->setChecked(true);

  query_mode_layout->addWidget(d->exact_match_radio);
  query_mode_layout->addWidget(d->containing_radio);

  connect(d->exact_match_radio, &QRadioButton::toggled,
          this, &EntityExplorer::QueryParametersChanged);

  connect(d->containing_radio, &QRadioButton::toggled,
          this, &EntityExplorer::QueryParametersChanged);

  search_parameters_layout->addLayout(query_mode_layout);

  auto layout = new QVBoxLayout;
  layout->setContentsMargins(0, 0, 0, 0);

  auto category_combo_box = new CategoryComboBox(d->view);
  connect(category_combo_box, &CategoryComboBox::CategoryChanged,
          this, &EntityExplorer::OnCategoryChanged);

  // Make the list generator that will show the results.
  d->list_widget = new ListGeneratorWidget(d->config_manager, kModelId, d->view);
  connect(d->list_widget, &ListGeneratorWidget::RequestSecondaryClick,
          manager, &IWindowManager::OnSecondaryClick);

  connect(d->list_widget, &ListGeneratorWidget::RequestPrimaryClick,
          manager, &IWindowManager::OnPrimaryClick);

  layout->addLayout(search_parameters_layout);
  layout->addWidget(category_combo_box);
  layout->addWidget(d->list_widget, 1);
  layout->addStretch();

  d->view->setContentsMargins(0, 0, 0, 0);
  d->view->setLayout(layout);

  IWindowManager::DockConfig config;
  config.id = "com.trailofbits.dock.EntityExplorer";
  config.app_menu_location = {tr("View"), tr("Explorers")};
  manager->AddDockWidget(d->view, config);

  // Add the `ctrl-shift-f` global shortcut.
  d->shortcut = new QShortcut(
      kKeqSeqCtrlShiftF, d->view, this,
      &EntityExplorer::OnSearchShortcutTriggered, Qt::ApplicationShortcut);
}

void EntityExplorer::OnSearchShortcutTriggered(void) {
  d->view->EmitRequestAttention();
  d->search_input->setFocus();
}

void EntityExplorer::ActOnPrimaryClick(
    IWindowManager *, const QModelIndex &index) {

  if (!d->view || !d->view->isVisible() || !index.isValid()) {
    return;
  }

  if (IModel::ModelId(index) == kModelId) {
    d->open_entity_trigger.Trigger(index.data(IModel::EntityRole));
  }
}

void EntityExplorer::ActOnContextMenu(IWindowManager *manager, QMenu *menu,
                                      const QModelIndex &index) {
  if (!d->view || !d->view->isVisible()) {
    return;
  }

  if (index.isValid() && IModel::ModelId(index) != kModelId) {
    return;
  }

  d->list_widget->ActOnContextMenu(manager, menu, index);
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
