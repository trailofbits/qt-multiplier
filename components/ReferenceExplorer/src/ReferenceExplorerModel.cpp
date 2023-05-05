/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "ReferenceExplorerModel.h"
#include "INodeGenerator.h"

#include <multiplier/ui/Assert.h>
#include <multiplier/ui/Util.h>

#include <multiplier/File.h>
#include <multiplier/Index.h>
#include <multiplier/AST.h>

#include <QApplication>
#include <QByteArray>
#include <QIODevice>
#include <QPalette>
#include <QString>
#include <QThreadPool>
#include <QColor>

#include <filesystem>

namespace mx::gui {

namespace {

static TokenCategory FromDeclCategory(DeclCategory cat) {
  switch (cat) {
    case DeclCategory::LOCAL_VARIABLE: return TokenCategory::LOCAL_VARIABLE;
    case DeclCategory::GLOBAL_VARIABLE: return TokenCategory::GLOBAL_VARIABLE;
    case DeclCategory::PARAMETER_VARIABLE:
      return TokenCategory::PARAMETER_VARIABLE;
    case DeclCategory::FUNCTION: return TokenCategory::FUNCTION;
    case DeclCategory::INSTANCE_METHOD: return TokenCategory::INSTANCE_METHOD;
    case DeclCategory::INSTANCE_MEMBER: return TokenCategory::INSTANCE_MEMBER;
    case DeclCategory::CLASS_METHOD: return TokenCategory::CLASS_METHOD;
    case DeclCategory::CLASS_MEMBER: return TokenCategory::CLASS_MEMBER;
    case DeclCategory::THIS: return TokenCategory::THIS;
    case DeclCategory::CLASS: return TokenCategory::CLASS;
    case DeclCategory::STRUCTURE: return TokenCategory::STRUCT;
    case DeclCategory::UNION: return TokenCategory::UNION;
    case DeclCategory::CONCEPT: return TokenCategory::CONCEPT;
    case DeclCategory::INTERFACE: return TokenCategory::INTERFACE;
    case DeclCategory::ENUMERATION: return TokenCategory::ENUM;
    case DeclCategory::ENUMERATOR: return TokenCategory::ENUMERATOR;
    case DeclCategory::NAMESPACE: return TokenCategory::NAMESPACE;
    case DeclCategory::TYPE_ALIAS: return TokenCategory::TYPE_ALIAS;
    case DeclCategory::TEMPLATE_TYPE_PARAMETER:
      return TokenCategory::TEMPLATE_PARAMETER_TYPE;
    case DeclCategory::TEMPLATE_VALUE_PARAMETER:
      return TokenCategory::TEMPLATE_PARAMETER_VALUE;
    case DeclCategory::LABEL: return TokenCategory::LABEL;
    default: return TokenCategory::UNKNOWN;
  }
}

}  // namespace

struct ReferenceExplorerModel::PrivateData final {
  PrivateData(const mx::Index &index_,
              const mx::FileLocationCache &file_location_cache_)
      : index(index_),
        file_location_cache(file_location_cache_) {

    for (auto [path, id] : index.file_paths()) {
      file_path_map.emplace(id.Pack(),
                            QString::fromStdString(path.generic_string()));
    }
  }

  mx::Index index;

  //! Caches file/line/column mappings for open files.
  mx::FileLocationCache file_location_cache;

  //! The path map from mx::Index, keyed by id
  std::unordered_map<RawEntityId, QString> file_path_map;

  //! Node tree for this model.
  NodeTree node_tree;
};

//! Adds a new entity object under the given parent
void ReferenceExplorerModel::AppendEntityById(RawEntityId entity_id,
                                              ExpansionMode expansion_mode,
                                              const QModelIndex &parent) {

  INodeGenerator *generator = INodeGenerator::CreateRootGenerator(
      d->index, d->file_location_cache, entity_id, parent, expansion_mode);

  if (!generator) {
    return;
  }

  generator->setAutoDelete(true);

  connect(generator, &INodeGenerator::NodesAvailable, this,
          &ReferenceExplorerModel::InsertNodes);

  connect(generator, &INodeGenerator::Finished, this,
          &ReferenceExplorerModel::InsertNodes);

  QThreadPool::globalInstance()->start(generator);
}

void ReferenceExplorerModel::ExpandEntity(const QModelIndex &index) {
  if (!index.isValid()) {
    return;
  }

  auto node_id = static_cast<std::uint64_t>(index.internalId());
  auto node_it = d->node_tree.node_map.find(node_id);
  if (node_it == d->node_tree.node_map.end()) {
    return;
  }

  auto &node = node_it->second;
  if (node.expanded) {
    return;
  }

  node.expanded = true;

  INodeGenerator *generator = INodeGenerator::CreateChildGenerator(
      d->index, d->file_location_cache, node_it->second.entity_id, index,
      node.expansion_mode);

  generator->setAutoDelete(true);

  connect(generator, &INodeGenerator::NodesAvailable, this,
          &ReferenceExplorerModel::InsertNodes);

  connect(generator, &INodeGenerator::Finished, this,
          &ReferenceExplorerModel::InsertNodes);

  QThreadPool::globalInstance()->start(generator);
}

void ReferenceExplorerModel::RemoveEntity(const QModelIndex &index) {
  if (!index.isValid()) {
    return;
  }

  std::uint64_t node_id = static_cast<std::uint64_t>(index.internalId());
  auto &node_map = d->node_tree.node_map;
  auto node_it = node_map.find(node_id);
  if (node_it == node_map.end()) {
    return;
  }

  Assert(node_id == node_it->second.node_id, "Out-of-sync node ids.");

  std::uint64_t parent_node_id = node_it->second.parent_node_id;

  auto parent_it = node_map.find(parent_node_id);

  // Check if we're removing the alternate root.
  if (node_id == d->node_tree.curr_root_node_id) {
    Assert(d->node_tree.curr_root_node_id != d->node_tree.root_node_id,
           "Can't remove the true root node.");
    d->node_tree.curr_root_node_id = d->node_tree.root_node_id;
  }

  QModelIndex parent_index = QModelIndex();
  int parent_offset = 0;
  if (parent_it == node_map.end()) {
    Assert(false, "Missing parent node, or removing true root node");

    // We're removing something inside of our parent.
  } else {
    Assert(parent_node_id == parent_it->second.node_id, "Out-of-sync node ids");

    auto found = false;
    for (auto sibling_id : parent_it->second.child_node_id_list) {
      if (sibling_id == node_id) {
        found = true;
        break;
      }
      ++parent_offset;
    }
    if (!found) {
      Assert(false, "Didn't find node to be deleted in parent's child list.");
    }

    if (parent_node_id != d->node_tree.curr_root_node_id) {
      parent_index = createIndex(parent_offset, 0, parent_node_id);
    }
  }

  emit beginResetModel();

  std::vector<std::uint64_t> wl;
  wl.push_back(node_id);

  // Recursively delete the child nodes.
  while (!wl.empty()) {
    std::uint64_t next_node_id = wl.back();
    Assert(next_node_id != parent_node_id, "Tree is actually a graph.");
    wl.pop_back();

    node_it = node_map.find(next_node_id);
    if (node_it == node_map.end()) {
      continue;
    }

    for (std::uint64_t child_node_id : node_it->second.child_node_id_list) {
      wl.push_back(child_node_id);
    }

    node_map.erase(node_it);
  }

  // Remove the node in its parent's list of child ids.
  if (parent_it != node_map.end()) {
    auto it = std::remove(parent_it->second.child_node_id_list.begin(),
                          parent_it->second.child_node_id_list.end(), node_id);
    parent_it->second.child_node_id_list.erase(
        it, parent_it->second.child_node_id_list.end());
  }

  emit endResetModel();
}

bool ReferenceExplorerModel::HasAlternativeRoot() const {
  return d->node_tree.root_node_id != d->node_tree.curr_root_node_id;
}

void ReferenceExplorerModel::SetRoot(const QModelIndex &index) {
  std::uint64_t root_node_id = d->node_tree.root_node_id;
  if (index.isValid()) {
    auto node_id_var =
        index.data(ReferenceExplorerModel::InternalIdentifierRole);

    if (node_id_var.isValid()) {
      root_node_id = node_id_var.toULongLong();
    }
  }

  emit beginResetModel();

  d->node_tree.curr_root_node_id = root_node_id;

  emit endResetModel();
}

void ReferenceExplorerModel::SetDefaultRoot() {
  SetRoot(QModelIndex());
}

ReferenceExplorerModel::~ReferenceExplorerModel() {}

QModelIndex ReferenceExplorerModel::index(int row, int column,
                                          const QModelIndex &parent) const {
  if (!hasIndex(row, column, parent)) {
    return QModelIndex();
  }

  std::uint64_t parent_node_id{d->node_tree.curr_root_node_id};
  if (parent.isValid()) {
    parent_node_id = static_cast<std::uint64_t>(parent.internalId());
  }

  auto parent_node_it = d->node_tree.node_map.find(parent_node_id);
  if (parent_node_it == d->node_tree.node_map.end()) {
    return QModelIndex();
  }

  const Node &parent_node = parent_node_it->second;

  auto unsigned_row = static_cast<std::size_t>(row);
  if (unsigned_row >= parent_node.child_node_id_list.size()) {
    return QModelIndex();
  }

  auto child_node_id = parent_node.child_node_id_list[unsigned_row];
  if (d->node_tree.node_map.count(child_node_id) == 0) {
    return QModelIndex();
  }

  return createIndex(row, column, static_cast<quintptr>(child_node_id));
}

QModelIndex ReferenceExplorerModel::parent(const QModelIndex &child) const {
  if (!child.isValid()) {
    return QModelIndex();
  }

  auto child_node_id = static_cast<std::uint64_t>(child.internalId());

  auto child_node_it = d->node_tree.node_map.find(child_node_id);
  if (child_node_it == d->node_tree.node_map.end()) {
    return QModelIndex();
  }

  const auto &child_node = child_node_it->second;
  if (child_node.parent_node_id == 0) {
    return QModelIndex();
  }

  auto parent_node_it = d->node_tree.node_map.find(child_node.parent_node_id);
  if (parent_node_it == d->node_tree.node_map.end()) {
    return QModelIndex();
  }

  const auto &parent_node = parent_node_it->second;

  auto grandparent_node_id = parent_node.parent_node_id;

  auto grandparent_node_it = d->node_tree.node_map.find(grandparent_node_id);
  if (grandparent_node_it == d->node_tree.node_map.end()) {
    return QModelIndex();
  }

  const auto &grandparent_node = grandparent_node_it->second;

  auto child_node_id_it = std::find(grandparent_node.child_node_id_list.begin(),
                                    grandparent_node.child_node_id_list.end(),
                                    child_node.parent_node_id);

  if (child_node_id_it == grandparent_node.child_node_id_list.end()) {
    return QModelIndex();
  }

  auto parent_row = static_cast<int>(std::distance(
      grandparent_node.child_node_id_list.begin(), child_node_id_it));

  return createIndex(parent_row, 0, child_node.parent_node_id);
}

int ReferenceExplorerModel::rowCount(const QModelIndex &parent) const {
  if (parent.column() > 1) {
    return 0;
  }

  std::uint64_t parent_node_id{d->node_tree.curr_root_node_id};
  if (parent.isValid()) {
    parent_node_id = static_cast<std::uint64_t>(parent.internalId());
  }

  auto parent_node_it = d->node_tree.node_map.find(parent_node_id);
  if (parent_node_it == d->node_tree.node_map.end()) {
    return 0;
  }

  const auto &parent_node = parent_node_it->second;
  return static_cast<int>(parent_node.child_node_id_list.size());
}

int ReferenceExplorerModel::columnCount(const QModelIndex &) const {
  if (d->node_tree.node_map.empty()) {
    return 0;
  }

  return 3;
}

QVariant ReferenceExplorerModel::data(const QModelIndex &index,
                                      int role) const {
  if (!index.isValid()) {
    return QVariant();
  }

  auto node_id = static_cast<std::uint64_t>(index.internalId());
  auto node_it = d->node_tree.node_map.find(node_id);
  if (node_it == d->node_tree.node_map.end()) {
    return QVariant();
  }

  const auto &node = node_it->second;

  QVariant value;

  // Make the text of the breadcrumbs and location slightly transparent, so that
  // they don't draw too much attention.
  if (role == Qt::ForegroundRole) {
    QColor color = qApp->palette().text().color();
    if (index.column() > 0) {
      return QColor::fromRgbF(color.redF(), color.greenF(), color.blueF(),
                              color.alphaF() * static_cast<float>(0.75));
    } else {
      return color;
    }

  } else if (role == Qt::DisplayRole) {
    auto column_number = index.column();

    if (column_number == 0) {
      if (node.opt_name.has_value()) {
        value = node.opt_name.value();

      } else {
        value = tr("Unnamed: ") + QString::number(node.entity_id);
      }

    } else if (column_number == 1) {
      if (node.opt_location.has_value()) {
        const auto &location = node.opt_location.value();

        std::filesystem::path file_path{location.path.toStdString()};
        auto file_name = QString::fromStdString(file_path.filename());

        value.setValue(QString("%1:%2:%3")
                           .arg(file_name, QString::number(location.line),
                                QString::number(location.column)));
      }

    } else if (column_number == 2) {
      if (node.opt_breadcrumbs.has_value()) {
        const auto &breadcrumbs = node.opt_breadcrumbs.value();
        value.setValue(breadcrumbs);
      }
    }

  } else if (role == Qt::ToolTipRole) {
    auto opt_decl_category = GetTokenCategory(d->index, node.entity_id);
    auto buffer =
        tr("Category: ") + GetTokenCategoryName(opt_decl_category) + "\n";

    buffer += tr("Entity ID: ") + QString::number(node.entity_id) + "\n";

    if (std::optional<FragmentId> frag_id =
            FragmentId::from(node.referenced_entity_id)) {

      buffer += tr("Fragment ID: ") +
                QString::number(EntityId(frag_id.value()).Pack());
    }

    if (node.opt_location.has_value()) {
      buffer += "\n";
      buffer += tr("File ID: ") + QString::number(node.opt_location->file_id);
    }

    if (node.opt_location.has_value()) {
      const auto &location = node.opt_location.value();

      buffer += "\n";
      buffer += tr("Path: ") + location.path;
    }

    value = std::move(buffer);

  } else if (role == IReferenceExplorerModel::EntityIdRole) {
    value = static_cast<quint64>(node.entity_id);

  } else if (role == IReferenceExplorerModel::ReferencedEntityIdRole) {
    value = static_cast<quint64>(node.referenced_entity_id);

  } else if (role == IReferenceExplorerModel::FragmentIdRole) {
    if (std::optional<FragmentId> frag_id =
            FragmentId::from(node.referenced_entity_id)) {
      value.setValue(EntityId(frag_id.value()).Pack());
    }

  } else if (role == IReferenceExplorerModel::FileIdRole) {
    if (node.opt_location.has_value()) {
      value.setValue(node.opt_location->file_id);
    }

  } else if (role == IReferenceExplorerModel::LineNumberRole) {
    if (node.opt_location.has_value() && 0u < node.opt_location->line) {
      value.setValue(node.opt_location->line);
    }

  } else if (role == IReferenceExplorerModel::ColumnNumberRole) {
    if (node.opt_location.has_value() && 0u < node.opt_location->column) {
      value.setValue(node.opt_location->column);
    }

  } else if (role == IReferenceExplorerModel::TokenCategoryRole) {
    value.setValue(TokenCategory::FUNCTION);

  } else if (role == IReferenceExplorerModel::LocationRole) {
    if (node.opt_location.has_value()) {
      value.setValue(node.opt_location.value());
    }

  } else if (role == ReferenceExplorerModel::InternalIdentifierRole) {
    value.setValue(node_id);

  } else if (role == IReferenceExplorerModel::ExpansionModeRole) {
    value.setValue(node.expansion_mode);

  } else if (role == IReferenceExplorerModel::ExpansionStatusRole) {
    value.setValue(node.expanded);

  } else if (role == ReferenceExplorerModel::IconLabelRole) {
    auto opt_decl_category = GetTokenCategory(d->index, node.entity_id);

    auto label = GetTokenCategoryIconLabel(opt_decl_category);
    value.setValue(label);

  } else if (role == ReferenceExplorerModel::ExpansionModeColor) {
    switch (node.expansion_mode) {
      case IReferenceExplorerModel::ExpansionMode::CallHierarchyMode:
        value.setValue(QColor(0x50, 0x00, 0x00));
        break;

      case IReferenceExplorerModel::ExpansionMode::TaintMode:
        value.setValue(QColor(0x00, 0x00, 0x50));
        break;
    }
  }

  return value;
}

QVariant ReferenceExplorerModel::headerData(int section,
                                            Qt::Orientation orientation,
                                            int role) const {

  if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
    return QVariant();
  }

  if (section == 0) {
    return tr("Entity");

  } else if (section == 1) {
    return tr("File name");

  } else {
    return tr("Breadcrumbs");
  }
}

void ReferenceExplorerModel::InsertNodes(QVector<Node> nodes, int row,
                                         const QModelIndex &drop_target) {

  // Figure out the drop target. This is the internal node id of the parent
  // node that will contain our dropped nodes.
  std::uint64_t drop_target_node_id = d->node_tree.curr_root_node_id;
  if (drop_target.isValid()) {
    auto drop_target_var =
        drop_target.data(ReferenceExplorerModel::InternalIdentifierRole);
    Assert(drop_target_var.isValid(), "Invalid InternalIdentifierRole value");
    drop_target_node_id = qvariant_cast<std::uint64_t>(drop_target_var);
  }

  if (!d->node_tree.node_map.contains(drop_target_node_id)) {
    return;
  }

  // Figure out where to drop the item in within the `drop_target_node_id`.
  int begin_row{};
  if (row != -1) {
    begin_row = row;
  } else if (drop_target.isValid()) {
    begin_row = drop_target.row();
  } else {
    begin_row = rowCount(QModelIndex());
  }

  // Link the new `root_nodes_dropped` into the parent node.
  Node &parent_node = d->node_tree.node_map[drop_target_node_id];
  Assert(parent_node.node_id == drop_target_node_id, "Invalid drop target");

  if (static_cast<size_t>(begin_row) > parent_node.child_node_id_list.size()) {
    return;
  }

  std::vector<std::uint64_t> root_nodes_dropped;

  // Create an old-to-new node ID mapping.
  std::unordered_map<std::uint64_t, std::uint64_t> id_mapping;
  for (Node &node : nodes) {
    const std::uint64_t old_id = node.node_id;
    Assert(old_id != 0u, "Invalid node id");
    node.AssignUniqueId();  // NOTE(pag): Replaces `Node::node_id`.
    Assert(node.node_id != 0u, "Invlaid unique node id");
    auto [it, added] = id_mapping.emplace(old_id, node.node_id);
    Assert(added, "Repeat node id found");
  }

  // Remap a node id as well as its parent node id. If the parent node id isn't
  // contained in our current tree, then this must be a node from the root of
  // what we were dragging. Rewrite the node's parent id to be the drop target
  // id.
  for (Node &node : nodes) {
    std::uint64_t parent_node_id = node.parent_node_id;
    auto parent_it = id_mapping.find(parent_node_id);
    if (parent_it == id_mapping.end()) {
      root_nodes_dropped.push_back(node.node_id);
      node.parent_node_id = drop_target_node_id;
    } else {
      node.parent_node_id = parent_it->second;
    }
  }

  // The `expanded` property of this node has changed, so tell the view
  // about it. This will disable the expand button (regardless of whether
  // we did get new nodes or not).
  emit dataChanged(drop_target, drop_target);

  // We did nothing, or we did nothing visible.
  auto num_dropped_nodes = static_cast<int>(root_nodes_dropped.size());
  if (!num_dropped_nodes) {
    return;
  }

  auto end_row = begin_row + static_cast<int>(root_nodes_dropped.size()) - 1;
  emit beginInsertRows(drop_target, begin_row, end_row);

  // Add the nodes into our tree.
  for (Node &node : nodes) {
    const std::uint64_t node_id = node.node_id;

    // Remap the child node ids.
    for (std::uint64_t &child_node_id : node.child_node_id_list) {
      child_node_id = id_mapping[child_node_id];
      Assert(child_node_id != 0u, "Missing child node");
    }

    auto [it, added] = d->node_tree.node_map.emplace(node_id, std::move(node));
    Assert(added, "Repeat node id found");
  }

  parent_node.child_node_id_list.insert(
      parent_node.child_node_id_list.begin() + static_cast<unsigned>(begin_row),
      root_nodes_dropped.begin(), root_nodes_dropped.end());

  emit endInsertRows();
}

ReferenceExplorerModel::ReferenceExplorerModel(
    const Index &index, const FileLocationCache &file_location_cache,
    QObject *parent)
    : IReferenceExplorerModel(parent),
      d(new PrivateData(index, file_location_cache)) {}

TokenCategory ReferenceExplorerModel::GetTokenCategory(const Index &index,
                                                       RawEntityId entity_id) {
  auto entity_var = index.entity(entity_id);
  if (std::holds_alternative<Decl>(entity_var)) {
    return FromDeclCategory(std::get<mx::Decl>(entity_var).category());

  } else if (std::holds_alternative<Macro>(entity_var)) {
    std::optional<Macro> m(std::move(std::get<Macro>(entity_var)));
    for (; m; m = m->parent()) {
      switch (m->kind()) {
        case MacroKind::DEFINE_DIRECTIVE: return TokenCategory::MACRO_NAME;
        case MacroKind::PARAMETER: return TokenCategory::MACRO_PARAMETER_NAME;
        case MacroKind::OTHER_DIRECTIVE:
        case MacroKind::IF_DIRECTIVE:
        case MacroKind::IF_DEFINED_DIRECTIVE:
        case MacroKind::IF_NOT_DEFINED_DIRECTIVE:
        case MacroKind::ELSE_IF_DIRECTIVE:
        case MacroKind::ELSE_IF_DEFINED_DIRECTIVE:
        case MacroKind::ELSE_IF_NOT_DEFINED_DIRECTIVE:
        case MacroKind::ELSE_DIRECTIVE:
        case MacroKind::END_IF_DIRECTIVE:
        case MacroKind::UNDEFINE_DIRECTIVE:
        case MacroKind::PRAGMA_DIRECTIVE:
        case MacroKind::INCLUDE_DIRECTIVE:
        case MacroKind::INCLUDE_NEXT_DIRECTIVE:
        case MacroKind::INCLUDE_MACROS_DIRECTIVE:
        case MacroKind::IMPORT_DIRECTIVE:
          return TokenCategory::MACRO_DIRECTIVE_NAME;
        default: break;
      }
    }
  } else if (std::holds_alternative<Token>(entity_var)) {
    return std::get<Token>(entity_var).category();
  }
  return TokenCategory::UNKNOWN;
}

const QString &
ReferenceExplorerModel::GetTokenCategoryIconLabel(TokenCategory tok_category) {

  // We can have up to four characters
  static const QString kInvalidCategory("Unk");

  // clang-format on
  static const std::unordered_map<TokenCategory, QString> kLabelMap{
      {TokenCategory::UNKNOWN, kInvalidCategory},
      {TokenCategory::LOCAL_VARIABLE, "Vr"},
      {TokenCategory::GLOBAL_VARIABLE, "GVa"},
      {TokenCategory::PARAMETER_VARIABLE, "Par"},
      {TokenCategory::FUNCTION, "Fn"},
      {TokenCategory::INSTANCE_METHOD, "Mt"},
      {TokenCategory::INSTANCE_MEMBER, "Fld"},
      {TokenCategory::CLASS_METHOD, "CFn"},
      {TokenCategory::CLASS_MEMBER, "CVr"},
      {TokenCategory::THIS, "t"},
      {TokenCategory::CLASS, "Cls"},
      {TokenCategory::STRUCT, "Str"},
      {TokenCategory::UNION, "Un"},
      {TokenCategory::CONCEPT, "Cpt"},
      {TokenCategory::INTERFACE, "Int"},
      {TokenCategory::ENUM, "EnT"},
      {TokenCategory::ENUMERATOR, "En"},
      {TokenCategory::NAMESPACE, "Ns"},
      {TokenCategory::TYPE_ALIAS, "Typ"},
      {TokenCategory::TEMPLATE_PARAMETER_TYPE, "TP"},
      {TokenCategory::TEMPLATE_PARAMETER_VALUE, "TP"},
      {TokenCategory::LABEL, "Lbl"},
      {TokenCategory::MACRO_DIRECTIVE_NAME, "Dir"},
      {TokenCategory::MACRO_NAME, "M"},
      {TokenCategory::MACRO_PARAMETER_NAME, "MP"},
  };
  // clang-format on

  auto label_map_it = kLabelMap.find(tok_category);
  if (label_map_it == kLabelMap.end()) {
    return kInvalidCategory;
  }

  return label_map_it->second;
}

const QString &
ReferenceExplorerModel::GetTokenCategoryName(TokenCategory tok_category) {

  static const QString kInvalidCategory = tr("Unknown");

  // clang-format off
  static const std::unordered_map<TokenCategory, QString> kLabelMap{
    { TokenCategory::UNKNOWN, kInvalidCategory },
    { TokenCategory::LOCAL_VARIABLE, tr("Local Variable") },
    { TokenCategory::GLOBAL_VARIABLE, tr("Global Variable") },
    { TokenCategory::PARAMETER_VARIABLE, tr("Parameter Variable") },
    { TokenCategory::FUNCTION, tr("Function") },
    { TokenCategory::INSTANCE_METHOD, tr("Instance Method") },
    { TokenCategory::INSTANCE_MEMBER, tr("Instance Member") },
    { TokenCategory::CLASS_METHOD, tr("Class Method") },
    { TokenCategory::CLASS_MEMBER, tr("Class Member") },
    { TokenCategory::THIS, tr("This") },
    { TokenCategory::CLASS, tr("Class") },
    { TokenCategory::STRUCT, tr("Structure") },
    { TokenCategory::UNION, tr("Union") },
    { TokenCategory::CONCEPT, tr("Concept") },
    { TokenCategory::INTERFACE, tr("Interface") },
    { TokenCategory::ENUM, tr("Enumeration") },
    { TokenCategory::ENUMERATOR, tr("Enumerator") },
    { TokenCategory::NAMESPACE, tr("Namespace") },
    { TokenCategory::TYPE_ALIAS, tr("Type Alias") },
    { TokenCategory::TEMPLATE_PARAMETER_TYPE, tr("Template Type Parameter") },
    { TokenCategory::TEMPLATE_PARAMETER_VALUE, tr("Template Value Parameter") },
    { TokenCategory::LABEL, tr("Label") },
    {TokenCategory::MACRO_DIRECTIVE_NAME, "Macro Directive"},
    {TokenCategory::MACRO_NAME, "Macro"},
    {TokenCategory::MACRO_PARAMETER_NAME, "Macro Parameter"},
  };
  // clang-format on

  auto label_map_it = kLabelMap.find(tok_category);
  if (label_map_it == kLabelMap.end()) {
    return kInvalidCategory;
  }

  return label_map_it->second;
}

}  // namespace mx::gui
