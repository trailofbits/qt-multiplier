// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "NameRenamesModel.h"
#include "RenameCommands.h"

#include <multiplier/GUI/Explorers/NameExplorer.h>
#include <multiplier/GUI/Interfaces/IModel.h>
#include <multiplier/GUI/Interfaces/IWindowManager.h>
#include <multiplier/GUI/Interfaces/IWindowWidget.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Util.h>

#include <multiplier/AST/Decl.h>
#include <multiplier/AST/NamedDecl.h>
#include <multiplier/Index.h>

#include <QAction>
#include <QCursor>
#include <QEvent>
#include <QHeaderView>
#include <QInputDialog>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollBar>
#include <QTableView>
#include <QUndoGroup>
#include <QUndoStack>
#include <QVBoxLayout>

#include <multiplier/GUI/Managers/MediaManager.h>
#include <multiplier/GUI/Interfaces/ITheme.h>

namespace mx::gui {

namespace {

class NameExplorerWindowWidget Q_DECL_FINAL : public IWindowWidget {
 public:
  NameExplorerWindowWidget(void) = default;

  void EmitRequestAttention(void) {
    emit RequestAttention();
  }
};

}  // namespace

struct NameExplorer::PrivateData {
  ConfigManager &config_manager;
  IWindowManager *manager{nullptr};
  NameExplorerWindowWidget *dock{nullptr};
  QTableView *view{nullptr};
  NameRenamesModel *model{nullptr};
  QUndoStack *undo_stack{nullptr};

  // Hover buttons (MacroExplorer pattern).
  QPushButton *goto_btn{nullptr};
  QPushButton *delete_btn{nullptr};
  QIcon goto_icon;
  QIcon delete_icon;
  bool updating_buttons{false};
  TriggerHandle goto_trigger;

  inline PrivateData(ConfigManager &config_manager_)
      : config_manager(config_manager_) {}
};

NameExplorer::~NameExplorer(void) {
  // Save renames on destruction.
  if (d->model) {
    d->config_manager.SaveRenamedEntities(d->model->buildRenameMap());
  }
}

NameExplorer::NameExplorer(ConfigManager &config_manager,
                           IWindowManager *parent)
    : IMainWindowPlugin(config_manager, parent),
      d(new PrivateData(config_manager)) {

  d->manager = parent;

  d->model = new NameRenamesModel(this);

  d->undo_stack = new QUndoStack(this);
  config_manager.UndoGroup().addStack(d->undo_stack);

  // Forward model changes to the RenameEntities signal.
  connect(d->model, &NameRenamesModel::renamesChanged,
          this, &NameExplorer::RenameEntities);

  connect(&config_manager, &ConfigManager::IndexChanged,
          this, &NameExplorer::OnIndexChanged);

  // Create the dock widget.
  d->dock = new NameExplorerWindowWidget;
  d->dock->setWindowTitle(tr("Names"));
  d->dock->setContentsMargins(0, 0, 0, 0);

  d->view = new QTableView(d->dock);
  d->view->setModel(d->model);
  d->view->setSelectionBehavior(QAbstractItemView::SelectRows);
  d->view->setSelectionMode(QAbstractItemView::SingleSelection);
  d->view->horizontalHeader()->setStretchLastSection(true);

  d->view->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(d->view, &QTableView::customContextMenuRequested,
          this, [this](const QPoint &pos) {
            auto index = d->view->indexAt(pos);
            if (!index.isValid()) {
              return;
            }

            auto row = static_cast<unsigned>(index.row());
            if (row >= d->model->entries().size()) {
              return;
            }

            const auto &entry = d->model->entries()[row];

            QMenu menu;
            auto *edit_action = menu.addAction(tr("Edit Name..."));
            auto *delete_action = menu.addAction(tr("Delete Rename"));

            auto *selected = menu.exec(d->view->viewport()->mapToGlobal(pos));
            if (selected == edit_action) {
              bool ok = false;
              auto new_name = QInputDialog::getText(
                  d->view, tr("Edit Rename"),
                  tr("New name for '%1':").arg(entry.original_name),
                  QLineEdit::Normal, entry.new_name, &ok);

              if (ok && !new_name.isEmpty() && new_name != entry.new_name) {
                d->config_manager.UndoGroup().setActiveStack(d->undo_stack);
                auto cmd_entry = entry;
                auto previous = entry.new_name;
                cmd_entry.new_name = new_name;
                d->undo_stack->push(new RenameCommand(
                    d->model, std::move(cmd_entry), std::move(previous)));
              }
            } else if (selected == delete_action) {
              d->config_manager.UndoGroup().setActiveStack(d->undo_stack);
              d->undo_stack->push(new RemoveRenameCommand(
                  d->model, entry));
            }
          });

  // Hover buttons.
  auto &media_manager = config_manager.MediaManager();

  d->goto_btn = new QPushButton(QIcon(), "", d->dock);
  d->goto_btn->setToolTip(tr("Go to Definition"));
  d->goto_icon.addPixmap(
      media_manager.Pixmap("com.trailofbits.icon.Activate"),
      QIcon::Normal, QIcon::On);
  d->goto_icon.addPixmap(
      media_manager.Pixmap("com.trailofbits.icon.Activate",
                           ITheme::IconStyle::DISABLED),
      QIcon::Disabled, QIcon::On);
  d->goto_btn->setIcon(d->goto_icon);

  d->delete_btn = new QPushButton(QIcon(), "", d->dock);
  d->delete_btn->setToolTip(tr("Remove Rename"));
  d->delete_icon.addPixmap(
      media_manager.Pixmap("com.trailofbits.icon.Close"),
      QIcon::Normal, QIcon::On);
  d->delete_icon.addPixmap(
      media_manager.Pixmap("com.trailofbits.icon.Close",
                           ITheme::IconStyle::DISABLED),
      QIcon::Disabled, QIcon::On);
  d->delete_btn->setIcon(d->delete_icon);

  // Register a trigger for navigating to entity definitions.
  d->goto_trigger = config_manager.ActionManager().Find(
      "com.trailofbits.action.OpenEntityPreview");

  connect(d->goto_btn, &QPushButton::pressed, this, [this] {
    auto mouse_pos = d->view->viewport()->mapFromGlobal(QCursor::pos());
    auto index = d->view->indexAt(mouse_pos);
    if (!index.isValid()) return;
    auto row = static_cast<unsigned>(index.row());
    if (row >= d->model->entries().size()) return;

    const auto &entry = d->model->entries()[row];
    auto entity = d->config_manager.Index().entity(EntityId(entry.canonical_id));
    if (!std::holds_alternative<NotAnEntity>(entity)) {
      if (std::holds_alternative<Decl>(entity)) {
        auto decl = std::get<Decl>(entity);
        if (auto def = decl.definition()) {
          entity = def.value();
        }
      }
      d->goto_trigger.Trigger(QVariant::fromValue(entity));
    }
  });

  connect(d->delete_btn, &QPushButton::pressed, this, [this] {
    auto mouse_pos = d->view->viewport()->mapFromGlobal(QCursor::pos());
    auto index = d->view->indexAt(mouse_pos);
    if (!index.isValid()) return;
    auto row = static_cast<unsigned>(index.row());
    if (row >= d->model->entries().size()) return;

    d->goto_btn->setVisible(false);
    d->delete_btn->setVisible(false);
    d->config_manager.UndoGroup().setActiveStack(d->undo_stack);
    d->undo_stack->push(new RemoveRenameCommand(
        d->model, d->model->entries()[row]));
  });

  d->view->installEventFilter(this);
  d->view->viewport()->installEventFilter(this);
  d->view->viewport()->setMouseTracking(true);

  auto dock_layout = new QVBoxLayout(d->dock);
  dock_layout->setContentsMargins(0, 0, 0, 0);
  dock_layout->addWidget(d->view, 1);
  d->dock->setLayout(dock_layout);

  IWindowManager::DockConfig config;
  config.tabify = true;
  config.start_hidden = true;
  config.id = "com.trailofbits.dock.NameExplorer";
  config.app_menu_location = {tr("View"), tr("Explorers")};
  d->manager->AddDockWidget(d->dock, config);

  // Load any saved renames. IndexChanged may have already fired.
  OnIndexChanged(config_manager);
}

QMap<RawEntityId, QString> NameExplorer::currentRenames(void) const {
  return d->model->buildRenameMap();
}

void NameExplorer::OnIndexChanged(const ConfigManager &config_manager) {
  d->model->clear();

  auto saved = config_manager.LoadRenamedEntities();
  if (saved.isEmpty()) {
    return;
  }

  const auto &index = config_manager.Index();
  FileLocationCache loc_cache = config_manager.FileLocationCache();

  // Group by canonical entity to reconstruct RenameEntry data.
  // The saved map is flat (all_ids -> name). We need to find canonical
  // entities and reconstruct the grouped entries.
  QMap<RawEntityId, RenameEntry> canonical_entries;

  for (auto it = saved.constBegin(); it != saved.constEnd(); ++it) {
    auto raw_id = it.key();
    auto &new_name = it.value();

    VariantEntity var_entity = index.entity(EntityId(raw_id));
    if (std::holds_alternative<NotAnEntity>(var_entity)) {
      continue;
    }

    if (!std::holds_alternative<Decl>(var_entity)) {
      continue;
    }

    auto decl = std::get<Decl>(var_entity).canonical_declaration();
    auto canonical_id = decl.id().Pack();

    if (canonical_entries.contains(canonical_id)) {
      continue;  // Already processed this group.
    }

    RenameEntry entry;
    entry.canonical_id = canonical_id;
    entry.new_name = new_name;

    if (auto named = NamedDecl::from(decl)) {
      entry.original_name = QString::fromStdString(
          std::string(named->name()));
      entry.kind = QString::fromStdString(
          std::string(EnumeratorName(decl.kind())));
    } else {
      entry.original_name = QStringLiteral("<unnamed>");
      entry.kind = QString::fromStdString(
          std::string(EnumeratorName(decl.kind())));
    }

    entry.location = LocationOfEntity(loc_cache, decl);

    for (auto redecl : decl.redeclarations()) {
      entry.all_ids.push_back(redecl.id().Pack());
    }

    canonical_entries.insert(canonical_id, std::move(entry));
  }

  for (auto &entry : canonical_entries) {
    d->model->addRename(entry.canonical_id, entry.original_name,
                        entry.new_name, entry.kind, entry.location,
                        entry.all_ids);
  }
}

void NameExplorer::OnDeleteSelected(void) {
  auto indices = d->view->selectionModel()->selectedRows();
  if (indices.isEmpty()) {
    return;
  }

  auto row = static_cast<unsigned>(indices.first().row());
  if (row >= d->model->entries().size()) {
    return;
  }

  d->config_manager.UndoGroup().setActiveStack(d->undo_stack);
  d->undo_stack->push(new RemoveRenameCommand(
      d->model, d->model->entries()[row]));
}

void NameExplorer::ActOnContextMenu(IWindowManager *, QMenu *menu,
                                    const QModelIndex &index) {
  auto var_entity = IModel::EntitySkipThroughTokens(index);
  if (std::holds_alternative<NotAnEntity>(var_entity)) {
    return;
  }

  if (!std::holds_alternative<Decl>(var_entity)) {
    return;
  }

  auto named = NamedDecl::from(var_entity);
  if (!named) {
    return;
  }

  auto decl = std::get<Decl>(var_entity).canonical_declaration();
  auto canonical_id = decl.id().Pack();

  QVector<RawEntityId> all_ids;
  for (auto redecl : decl.redeclarations()) {
    all_ids.push_back(redecl.id().Pack());
  }

  auto original_name = QString::fromStdString(std::string(named->name()));
  auto kind = QString::fromStdString(
      std::string(EnumeratorName(decl.kind())));
  auto location = LocationOfEntity(
      d->config_manager.FileLocationCache(), decl);

  // Check if there's already a rename for this entity.
  int existing_row = d->model->findRow(canonical_id);
  QString current_name = original_name;
  if (existing_row >= 0) {
    current_name = d->model->entries()[static_cast<unsigned>(existing_row)]
                       .new_name;
  }

  auto *rename_action = new QAction(tr("Rename Entity..."), menu);
  menu->addAction(rename_action);

  connect(rename_action, &QAction::triggered, this,
          [=, this](void) {
            bool ok = false;
            auto new_name = QInputDialog::getText(
                d->dock, tr("Rename Entity"),
                tr("New name for '%1':").arg(original_name),
                QLineEdit::Normal, current_name, &ok);

            if (!ok || new_name.isEmpty() || new_name == current_name) {
              return;
            }

            RenameEntry entry;
            entry.canonical_id = canonical_id;
            entry.original_name = original_name;
            entry.new_name = new_name;
            entry.kind = kind;
            entry.location = location;
            entry.all_ids = all_ids;

            QString previous_name;
            if (existing_row >= 0) {
              previous_name = current_name;
            }

            d->config_manager.UndoGroup().setActiveStack(d->undo_stack);
            d->undo_stack->push(new RenameCommand(
                d->model, std::move(entry), std::move(previous_name)));

            d->dock->show();
            d->dock->EmitRequestAttention();
          });
}

bool NameExplorer::eventFilter(QObject *obj, QEvent *event) {
  if (obj == d->view) {
    if (event->type() == QEvent::Wheel ||
        event->type() == QEvent::Resize) {
      UpdateItemButtons();
    }
  } else if (obj == d->view->viewport()) {
    if (event->type() == QEvent::Leave ||
        event->type() == QEvent::MouseMove) {
      UpdateItemButtons();
    }
  }
  return false;
}

void NameExplorer::UpdateItemButtons(void) {
  if (d->updating_buttons) {
    return;
  }

  d->updating_buttons = true;
  d->goto_btn->setVisible(false);
  d->delete_btn->setVisible(false);

  auto mouse_pos = d->view->viewport()->mapFromGlobal(QCursor::pos());
  auto index = d->view->indexAt(mouse_pos);
  if (!index.isValid()) {
    d->updating_buttons = false;
    return;
  }

  d->goto_btn->setVisible(true);
  d->delete_btn->setVisible(true);

  static constexpr auto kNumButtons = 2u;
  QPushButton *buttons[kNumButtons] = {d->goto_btn, d->delete_btn};

  auto rect = d->view->visualRect(index);
  auto button_margin = rect.height() / 6;
  auto button_size = rect.height() - (button_margin * 2);
  auto button_count = static_cast<int>(kNumButtons);
  auto button_area_width =
      (button_count * button_size) + (button_count * button_margin);

  auto current_x =
      d->view->pos().x() + d->view->width() - button_area_width;

  if (d->view->verticalScrollBar()->isVisible()) {
    current_x -= d->view->verticalScrollBar()->width();
  }

  auto current_y = rect.y() + (rect.height() / 2) - (button_size / 2);

  auto pos = d->view->viewport()->mapToGlobal(QPoint(current_x, current_y));
  pos = d->dock->mapFromGlobal(pos);

  current_x = pos.x();
  current_y = pos.y();

  for (auto *button : buttons) {
    button->resize(button_size, button_size);
    button->move(current_x, current_y);
    button->raise();
    current_x += button_size + button_margin;
  }

  d->updating_buttons = false;
}

}  // namespace mx::gui
