/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "EntityInformationWidget.h"

#include <QCheckBox>
#include <QElapsedTimer>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QThreadPool>
#include <QToolBar>
#include <QTreeView>
#include <QVBoxLayout>

#include <multiplier/AST/AddrLabelExpr.h>
#include <multiplier/AST/CallExpr.h>
#include <multiplier/AST/DeclRefExpr.h>
#include <multiplier/AST/FunctionDecl.h>
#include <multiplier/AST/LabelDecl.h>
#include <multiplier/AST/LabelStmt.h>
#include <multiplier/AST/MemberExpr.h>
#include <multiplier/Frontend/DefineMacroDirective.h>
#include <multiplier/Frontend/File.h>
#include <multiplier/Frontend/MacroExpansion.h>
#include <multiplier/Frontend/Token.h>
#include <multiplier/GUI/Managers/ActionManager.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/MediaManager.h>
#include <multiplier/GUI/Widgets/HistoryWidget.h>
#include <multiplier/GUI/Widgets/SearchWidget.h>
#include <multiplier/GUI/Widgets/TreeWidget.h>

#include "EntityInformationModel.h"
#include "EntityInformationRunnable.h"
#include "SortFilterProxyModel.h"

namespace mx::gui {
namespace {

static constexpr unsigned kMaxHistorySize = 32;

bool ShouldAutoExpand(const QModelIndex &index) {
  if (!index.isValid()) {
    return true;
  }

  auto auto_expand_var = index.data(EntityInformationModel::AutoExpandRole);
  if (!auto_expand_var.isValid()) {
    return true;
  }

  return auto_expand_var.toBool();
}

}  // namespace

struct EntityInformationWidget::PrivateData {
  
  // Used kind of like a semaphore to signal to info-fetching runnables
  // (executing in `thread_pool`) that they should stop early because their
  // results are going to be ignored / now out-of-date w.r.t. the current
  // entity being shown.
  const AtomicU64Ptr version_number;

  // Tree of entity info.
  TreeWidget * const tree;

  // Status indicator. Shown when `num_requests` is greater than zero.
  QWidget * const status;

  // Model that manages the tree of data for this entity information widget.
  EntityInformationModel * const model;

  // Model that enables filtering. Works with the `search` widget.
  SortFilterProxyModel * const sort_model;

  // Toolbar of buttons.
  QToolBar * const toolbar;

  // Widget keeping track of the history of the entity information browser. May
  // be `nullptr`.
  HistoryWidget * const history;

  // Used to pop out a copy of the current entity info into a pinned info
  // browser. May be `nullptr`.
  QPushButton * const pop_out_button;

  // Used to search through info results.
  SearchWidget * const search;

  // Thread pool on which the information fetching runnables run.
  QThreadPool thread_pool;

  // Current entity being shown by this widget.
  VariantEntity current_entity;

  // Should we show a checkbox and synchronize this info explorer with
  bool sync{true};

  // Number of open/pending requests. This helps us decide whether or not to
  // show the `Updating...` status indicator and the `Cancel` button.
  int num_requests{0};

  // Trigger to open some info in a pinned explorer.
  TriggerHandle pinned_entity_info_trigger;

  // The most recently selected `QModelIndex`, as well as a timer from
  // preventing us from raising duplicate signals, e.g. from `clicked` vs.
  // `selectionChanged`.
  QModelIndex selected_index;
  QElapsedTimer selection_timer;

  inline PrivateData(const ConfigManager &config_manager, bool enable_history,
                     QWidget *parent)
      : version_number(std::make_shared<AtomicU64>()),
        tree(new TreeWidget(parent)),
        status(new QWidget(parent)),
        model(new EntityInformationModel(
            config_manager.FileLocationCache(), version_number, tree)),
        sort_model(new SortFilterProxyModel(tree)),
        toolbar(enable_history ? new QToolBar(parent) : nullptr),
        history(
            enable_history ?
            new HistoryWidget(config_manager, kMaxHistorySize, false, toolbar) :
            nullptr),
        pop_out_button(enable_history ? new QPushButton(toolbar) : nullptr),
        search(new SearchWidget(config_manager.MediaManager(),
                                SearchWidget::Mode::Filter, parent)),
        pinned_entity_info_trigger(config_manager.ActionManager().Find(
            "com.trailofbits.action.OpenPinnedEntityInfo")) {}
};

EntityInformationWidget::~EntityInformationWidget(void) {}

EntityInformationWidget::EntityInformationWidget(
    const ConfigManager &config_manager, bool enable_history,
    QWidget *parent)
    : IWindowWidget(parent),
      d(new PrivateData(config_manager, enable_history, this)) {

  d->selection_timer.start();

  d->sort_model->setRecursiveFilteringEnabled(true);
  d->sort_model->setSourceModel(d->model);

  setWindowTitle(tr("Information Explorer"));
  d->tree->setModel(d->sort_model);
  d->tree->setHeaderHidden(true);
  d->tree->setSortingEnabled(true);
  d->tree->sortByColumn(0, Qt::AscendingOrder);

  d->tree->setContextMenuPolicy(Qt::CustomContextMenu);
  d->tree->viewport()->installEventFilter(this);

  // Create the status widget
  d->status->setVisible(false);

  auto status_layout = new QHBoxLayout(this);
  status_layout->setContentsMargins(0, 0, 0, 0);

  status_layout->addWidget(new QLabel(tr("Updating..."), this));
  status_layout->addStretch();

  auto cancel_button = new QPushButton(tr("Cancel"), this);
  status_layout->addWidget(cancel_button);

  connect(cancel_button, &QPushButton::pressed,
          this, &EntityInformationWidget::OnCancelRunningRequest);

  d->status->setLayout(status_layout);

  auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  if (d->toolbar) {
    d->toolbar->addWidget(d->history);
    d->toolbar->setIconSize(QSize(16, 16));
    d->history->SetIconSize(d->toolbar->iconSize());

    // Add a popout icon, to pop the current info into a pinned browser.
    auto &media_manager = config_manager.MediaManager();
    OnIconsChanged(media_manager);
    d->toolbar->addWidget(new QLabel(" "));
    d->toolbar->addWidget(d->pop_out_button);
    d->pop_out_button->setEnabled(false);
    d->pop_out_button->setToolTip(
        tr("Duplicate this information into a pinned info explorer"));

    // Create a sync checkbox that tells us whether or not to keep this entity
    // information explorer up-to-date with user clicks.
    auto sync = new QCheckBox(tr("Sync"), this);
    sync->setTristate(false);
    sync->setCheckState(Qt::Checked);
    d->toolbar->addWidget(new QLabel(" "));
    d->toolbar->addWidget(sync);

#ifndef QT_NO_TOOLTIP
    sync->setToolTip(tr("Keep in sync with clicks in other views"));
#endif

    connect(d->history, &HistoryWidget::GoToEntity, this,
            [this] (VariantEntity original_entity, VariantEntity) {
              emit HistoricalEntitySelected(std::move(original_entity));
            });

    connect(sync, &QCheckBox::stateChanged,
            this, &EntityInformationWidget::OnChangeSync);

    connect(&media_manager, &MediaManager::IconsChanged,
            this, &EntityInformationWidget::OnIconsChanged);

    connect(d->pop_out_button, &QPushButton::pressed,
            this, &EntityInformationWidget::OnPopOutPressed);

    layout->addWidget(d->toolbar);
  }

  connect(d->search, &SearchWidget::SearchParametersChanged,
          this, &EntityInformationWidget::OnSearchParametersChange);

  layout->addWidget(d->tree, 1);
  layout->addStretch();
  layout->addWidget(d->status);
  layout->addWidget(d->search);

  setContentsMargins(0, 0, 0, 0);
  setLayout(layout);

  config_manager.InstallItemDelegate(d->tree);

  connect(&config_manager, &ConfigManager::IndexChanged,
          d->model, &EntityInformationModel::OnIndexChanged);

  connect(d->sort_model, &QAbstractItemModel::rowsInserted,
          this, &EntityInformationWidget::ExpandAllBelow);
}

void EntityInformationWidget::OnPopOutPressed(void) {
  d->pinned_entity_info_trigger.Trigger(QVariant::fromValue(d->current_entity));
}

void EntityInformationWidget::OnIconsChanged(
    const MediaManager &media_manager) {
  if (!d->pop_out_button) {
    return;
  }

  QIcon pop_out_icon;
  pop_out_icon.addPixmap(media_manager.Pixmap("com.trailofbits.icon.PopOut"),
                                              QIcon::Normal, QIcon::On);
  
  pop_out_icon.addPixmap(media_manager.Pixmap("com.trailofbits.icon.PopOut",
                                              ITheme::IconStyle::DISABLED),
                                              QIcon::Disabled, QIcon::On);

  d->pop_out_button->setIcon(pop_out_icon);
  d->pop_out_button->setIconSize(d->toolbar->iconSize());
}

void EntityInformationWidget::OnSearchParametersChange(void) {
  
  QRegularExpression::PatternOptions options{
      QRegularExpression::NoPatternOption};

  auto &search_parameters = d->search->Parameters();
  if (!search_parameters.case_sensitive) {
    options |= QRegularExpression::CaseInsensitiveOption;
  }

  auto pattern = QString::fromStdString(search_parameters.pattern);

  if (search_parameters.type == SearchWidget::SearchParameters::Type::Text) {
    pattern = QRegularExpression::escape(pattern);
    if (search_parameters.whole_word) {
      pattern = "\\b" + pattern + "\\b";
    }
  }

  QRegularExpression regex(pattern, options);

  // The regex is already validated by the search widget
  Q_ASSERT(regex.isValid());

  d->sort_model->setFilterRegularExpression(regex);
  d->tree->expandRecursively(QModelIndex());
  d->tree->resizeColumnToContents(0);
}

void EntityInformationWidget::ExpandAllBelow(const QModelIndex &parent) {
  std::vector<QModelIndex> next_queue;
  next_queue.emplace_back(parent);

  while (!next_queue.empty()) {
    auto queue = std::move(next_queue);
    next_queue.clear();

    for (const auto &index : queue) {
      if (!ShouldAutoExpand(index)) {
        continue;
      }

      d->tree->expand(index);

      auto row_count = d->sort_model->rowCount(index);
      for (int row{}; row < row_count; ++row) {
        auto child_index = d->sort_model->index(row, 0, index);
        next_queue.push_back(child_index);
      }
    }
  }

  d->tree->resizeColumnToContents(0);
}

void EntityInformationWidget::DisplayEntity(
    VariantEntity entity, const FileLocationCache &file_location_cache,
    const std::vector<IInformationExplorerPluginPtr> &plugins,
    bool is_explicit_request, bool add_to_history) {

  // If we don't have this info browser synchronized with implicit events, then
  // ignore this request to display the entity.
  if (!is_explicit_request && !d->sync) {
    return;
  }

  if (std::holds_alternative<NotAnEntity>(entity)) {
    return;
  }

  if (auto tok = Token::from(entity)) {
    auto re = tok->related_entity();
    if (!std::holds_alternative<NotAnEntity>(re)) {
      entity = std::move(re);

    } else if (auto file = File::containing(tok.value())) {
      entity = std::move(file.value());
    }
  }

  // Follow through references. This isn't exactly pleasant, and doesn't quite
  // work right.
  //
  // TODO(pag): Generalize this.
  if (auto exp = MacroExpansion::from(entity)) {
    if (auto def = exp->definition()) {
      entity = std::move(def.value());
    }
  
  } else if (auto dre = DeclRefExpr::from(entity)) {
    entity = dre->declaration();
  
  } else if (auto me = MemberExpr::from(entity)) {
    entity = me->member_declaration();

  } else if (auto ale = AddrLabelExpr::from(entity)) {
    entity = ale->label();
  
  } else if (auto ls = LabelStmt::from(entity)) {
    entity = ls->declaration();

  } else if (auto ce = CallExpr::from(entity)) {
    if (auto dc = ce->direct_callee()) {
      entity = std::move(dc.value());
    }
  }

  // Canonicalize decls so that we can dedup check.
  if (std::holds_alternative<Decl>(entity)) {
    entity = std::get<Decl>(entity).canonical_declaration();
  }

  // Dedup check; don't want to reload the model unnecessarily.
  if (EntityId(d->current_entity) == EntityId(entity)) {
    return;
  }

  auto found = false;

  for (const auto &plugin : plugins) {
    auto category_generators = plugin->CreateInformationCollectors(entity);
    for (auto category_generator : category_generators) {
      if (!category_generator) {
        continue;
      }

      // Only clear the current view if one of the generators produces
      // something.
      if (!found) {
        found = true;
        d->current_entity = entity;
        d->model->Clear();

        if (d->pop_out_button) {
          d->pop_out_button->setEnabled(true);
        }

        // If we're showing the history widget then keep track of the history.
        if (add_to_history && d->history) {
          d->history->CommitCurrentLocationToHistory();
          d->history->SetCurrentLocation(d->current_entity);
        }
      }

      auto runnable = new EntityInformationRunnable(
          std::move(category_generator), file_location_cache,
          d->version_number);

      connect(runnable, &EntityInformationRunnable::NewGeneratedItems,
              d->model, &EntityInformationModel::AddData);

      connect(runnable, &EntityInformationRunnable::Finished,
              this, &EntityInformationWidget::OnAllDataFound);

      // Show the status widget (allowing us to cancel the request) if there
      // are any outstanding background requests.
      if (!d->status->isVisible()) {
        d->status->setVisible(true);
      }

      ++d->num_requests;
      d->thread_pool.start(runnable);
    }
  }
}

bool EntityInformationWidget::eventFilter(QObject *object, QEvent *event) {
  if (object == d->tree->viewport()) {
    if (event->type() == QEvent::MouseButtonPress) {
      return true;

    } else if (event->type() == QEvent::MouseButtonRelease) {
      const auto &mouse_event = *static_cast<QMouseEvent *>(event);
      auto local_mouse_pos = mouse_event.position().toPoint();

      auto index = d->tree->indexAt(local_mouse_pos);
      if (!index.isValid()) {
        return true;
      }

      auto &selection_model = *d->tree->selectionModel();
      selection_model.setCurrentIndex(index,
          QItemSelectionModel::Clear | QItemSelectionModel::SelectCurrent);

      switch (mouse_event.button()) {
      case Qt::LeftButton: {
        OnCurrentItemChanged(index);
        break;
      }

      case Qt::RightButton: {
        OnOpenItemContextMenu(local_mouse_pos);
        break;
      }

      default:
        break;
      }

      return true;

    } else {
      return false;
    }

  } else {
    return false;
  }
}

void EntityInformationWidget::OnAllDataFound(void) {
  --d->num_requests;

  if (!d->num_requests) {
    d->status->setVisible(false);
  }

  Q_ASSERT(d->num_requests >= 0);
}

void EntityInformationWidget::OnCancelRunningRequest(void) {
  d->status->setVisible(false);
  d->version_number->fetch_add(1u);
}

void EntityInformationWidget::OnChangeSync(int state) {
  d->sync = Qt::Checked == state;
}

void EntityInformationWidget::OnCurrentItemChanged(
    const QModelIndex &current_index) {

  // auto [time_diff, event_kind] = d->tree->GetLastMouseEvent();
  // qDebug() << time_diff << event_kind;

  auto new_index = d->sort_model->mapToSource(current_index);
  if (!new_index.isValid()) {
    return;
  }
  
  // Suppress likely duplicate events.
  if (d->selection_timer.restart() < 100 &&
      d->selected_index == new_index) {
    return;
  }

  d->selected_index = new_index;
  emit SelectedItemChanged(d->selected_index);
}

void EntityInformationWidget::OnOpenItemContextMenu(const QPoint &tree_local_mouse_pos) {
  auto index = d->tree->indexAt(tree_local_mouse_pos);
  d->selected_index = d->sort_model->mapToSource(index);
  if (!d->selected_index.isValid()) {
    return;
  }

  emit RequestSecondaryClick(d->selected_index);
}

}  // namespace mx::gui
