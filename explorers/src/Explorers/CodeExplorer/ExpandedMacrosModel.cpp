// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "ExpandedMacrosModel.h"

#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/ThemeManager.h>
#include <multiplier/GUI/Util.h>
#include <multiplier/Index.h>
#include <vector>

namespace mx::gui {

struct ExpandedMacrosModel::PrivateData {
  std::vector<Macro> macros;
  std::vector<TokenRange> tokens;
  std::vector<QString> display;
  std::vector<QString> location;

  FileLocationCache file_location_cache;

  QColor fg_color_role;
  QColor bg_color_role;
  QFont font_role;
};

ExpandedMacrosModel::~ExpandedMacrosModel(void) {}

ExpandedMacrosModel::ExpandedMacrosModel(const ConfigManager &config_manager,
                                         QObject *parent)
    : IModel(parent),
      d(new PrivateData) {

  OnIndexChanged(config_manager);

  connect(&config_manager, &ConfigManager::IndexChanged,
          this, &ExpandedMacrosModel::OnIndexChanged);

  auto &theme_manager = config_manager.ThemeManager();
  OnThemeChanged(theme_manager);
}

void ExpandedMacrosModel::OnIndexChanged(const ConfigManager &config_manager) {
  emit beginResetModel();
  d->macros.clear();
  d->tokens.clear();
  d->display.clear();
  d->location.clear();
  d->file_location_cache = config_manager.FileLocationCache();
  emit endResetModel();
}

void ExpandedMacrosModel::OnThemeChanged(const ThemeManager &theme_manager) {
  emit beginResetModel();
  auto theme = theme_manager.Theme();
  d->fg_color_role = theme->DefaultForegroundColor();
  d->bg_color_role = theme->DefaultBackgroundColor();
  d->font_role = theme->Font();
  emit endResetModel();
}

QModelIndex ExpandedMacrosModel::index(int row, int column,
                                       const QModelIndex &parent) const {

  if (!hasIndex(row, column, parent)) {
    return {};
  }

  if (parent.isValid() || 0 > column || 2 < column) {
    return {};
  }

  auto row_count = static_cast<int>(d->macros.size());
  if (0 > row || row >= row_count) {
    return {};
  }

  return createIndex(row, column);
}

QModelIndex ExpandedMacrosModel::parent(const QModelIndex &) const {
  return QModelIndex();
}

int ExpandedMacrosModel::rowCount(const QModelIndex &parent) const {
  return !parent.isValid() ? static_cast<int>(d->macros.size()) : 0;
}

int ExpandedMacrosModel::columnCount(const QModelIndex &parent) const {
  return !parent.isValid() ? 3 : 0;
}

QVariant ExpandedMacrosModel::headerData(
    int section, Qt::Orientation orientation, int role) const {
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
    if (section == 0) {
      return tr("Macro");
    } else if (section == 1) {
      return tr("Kind");
    } else if (section == 2) {
      return tr("Location");
    }
  }
  return {};
}

QVariant ExpandedMacrosModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid()) {
    return {};
  }

  auto row = static_cast<unsigned>(index.row());
  if (row >= d->macros.size()) {
    return {};
  } 

  auto col = index.column();
  const Macro &macro = d->macros[row];
  const TokenRange &tokens = d->tokens[row];
  const QString &display = d->display[row];
  const QString &location = d->location[row];

  if (role == Qt::DisplayRole) {
    if (col == 0) {
      return display;
    } else if (col == 1) {
      return QString(EnumeratorName(macro.kind()));
    } else if (col == 2) {
      return location;
    }

  // Tooltip used for hovering. Also, this is used for the copy details.
  } else if (role == Qt::ToolTipRole) {
    return tr("Macro Id: %1\nMacro Use: %2\nLocation: %3")
               .arg(macro.id().Pack())
               .arg(display)
               .arg(location);

  } else if (role == IModel::CopyableRoleMapIdRole) {
    QString label;
    switch (col) {
      case 0:
        label = tr("Entity Name");
        break;

      case 1:
        label = tr("Kind");
        break;

      case 2:
        label = tr("Location");
        break;

      default:
        label = tr("Unimplemented label for column ") + QString::number(col);
        break;
    }

    CopyableRoleMap copyable_role_map{
      { tr("Summary"), Qt::ToolTipRole },
      { label, Qt::DisplayRole },
    };

    QVariant value;
    value.setValue(copyable_role_map);

    return value;

  } else if (role == IModel::EntityRole) {
    return QVariant::fromValue<VariantEntity>(macro);

  } else if (role == IModel::TokenRangeDisplayRole) {
    if (col == 0) {
      return QVariant::fromValue<TokenRange>(tokens);
    }

  } else if (role == IModel::ModelIdRole) {
    return "com.trailofbits.explorer.MacroExplorer.ExpandedMacrosModel";
  
  } else if (role == Qt::BackgroundRole) {
    return d->bg_color_role;

  } else if (role == Qt::ForegroundRole) {
    return d->fg_color_role;
  
  } else if (role == Qt::FontRole) {
    return d->font_role;
  }

  return {};
}

void ExpandedMacrosModel::AddMacro(Macro macro) {
  QSet<RawEntityId> macro_ids;
  for (auto &existing_macro : d->macros) {
    macro_ids.insert(existing_macro.id().Pack());
  }

  auto macro_id = macro.id().Pack();
  if (macro_ids.contains(macro_id)) {
    return;  // Already being expanded.
  }

  macro_ids.insert(macro_id);

  emit beginInsertRows({}, 0, 0);

  std::reverse(d->macros.begin(), d->macros.end());
  std::reverse(d->tokens.begin(), d->tokens.end());
  std::reverse(d->display.begin(), d->display.end());
  std::reverse(d->location.begin(), d->location.end());

  if (auto def = DefineMacroDirective::from(macro)) {
    d->tokens.emplace_back(def->name());

  } else {
    d->tokens.emplace_back(macro.use_tokens());
  }

  d->display.emplace_back(TokensToString(d->tokens.back()));
  d->location.emplace_back(LocationOfEntity(d->file_location_cache, macro));
  d->macros.emplace_back(std::move(macro));

  std::reverse(d->macros.begin(), d->macros.end());
  std::reverse(d->tokens.begin(), d->tokens.end());
  std::reverse(d->display.begin(), d->display.end());
  std::reverse(d->location.begin(), d->location.end());

  emit endInsertRows();

  emit ExpandMacros(macro_ids);
}

void ExpandedMacrosModel::RemoveMacro(Macro macro) {
  auto macro_id = macro.id().Pack();

  std::vector<Macro> new_macros;
  std::vector<TokenRange> new_tokens;
  std::vector<QString> new_display;
  std::vector<QString> new_location;

  QSet<RawEntityId> macro_ids;
  auto row = 0u;
  std::optional<int> found_at_row;

  for (auto &existing_macro : d->macros) {
    auto existing_macro_id = existing_macro.id().Pack();
    if (existing_macro_id == macro_id) {
      Q_ASSERT(!found_at_row);
      found_at_row = static_cast<int>(row);
      continue;
    }

    macro_ids.insert(existing_macro_id);
    new_macros.emplace_back(existing_macro);
    new_tokens.emplace_back(d->tokens[row]);
    new_display.emplace_back(d->display[row]);
    new_location.emplace_back(d->location[row]);
    ++row;
  }

  if (!found_at_row) {
    return;
  }

  emit beginRemoveRows({}, found_at_row.value(), found_at_row.value());
  new_macros.swap(d->macros);
  new_tokens.swap(d->tokens);
  new_display.swap(d->display);
  new_location.swap(d->location);
  emit endRemoveRows();

  emit ExpandMacros(macro_ids);
}

// void ExpandedMacrosModel::RemoveEntity(const std::vector<RawEntityId> &eids) {
//   std::vector<VariantEntity> new_entities;
//   emit beginResetModel();
//   for (auto &entity : d->entities) {
//     auto eid = ::mx::EntityId(entity).Pack();
//     if (std::find(eids.begin(), eids.end(), eid) == eids.end()) {
//       new_entities.emplace_back(std::move(entity));
//     }
//   }
//   d->entities.swap(new_entities);
//   emit endResetModel();
// }

}  // namespace mx::gui
