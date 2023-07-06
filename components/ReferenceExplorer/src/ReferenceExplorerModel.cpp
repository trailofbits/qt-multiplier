/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "ReferenceExplorerModel.h"

#include <multiplier/ui/Assert.h>

#include <QFuture>
#include <QFutureWatcher>
#include <QTimer>
#include <QColor>
#include <QApplication>
#include <QPalette>

#include <iostream>

namespace mx::gui {

namespace {

const int kFirstUpdateInterval{500};
const int kImportInterval{1500};
const std::size_t kQueryDepth{2};

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
        file_location_cache(file_location_cache_) {}

  //! The multiplier database
  mx::Index index;

  //! Caches file/line/column mappings for open files.
  mx::FileLocationCache file_location_cache;

  //! The path map from mx::Index, keyed by id
  std::unordered_map<RawEntityId, QString> file_path_map;

  //! Database object used to request the nodes
  IDatabase::Ptr database;

  //! Database request future
  QFuture<bool> request_status_future;

  //! Database future watcher
  QFutureWatcher<bool> future_watcher;

  //! A list of QueryEntityReferencesResult objects that needs to be imported
  DataBatch data_batch_queue;

  //! A mutex protecting data_batch_queue
  std::mutex data_batch_queue_mutex;

  //! A timer used to import data from the data batch queue
  QTimer import_timer;

  //! The insert point for the active request
  QModelIndex insert_point;

  //! ID mapping info used by the active reques
  std::unordered_map<std::uint64_t, Context::NodeID> node_id_mapping;

  //! Reference type
  IDatabase::ReferenceType reference_type;

  //! Model data
  Context context;
};

ReferenceExplorerModel::Context::NodeID
ReferenceExplorerModel::GenerateNodeID(Context &context) {
  return ++context.node_id_generator;
}

void ReferenceExplorerModel::ResetContext(Context &context) {
  context = {};
}

void ReferenceExplorerModel::SetEntity(const RawEntityId &entity_id,
                                       const ReferenceType &reference_type) {
  CancelRunningRequest();

  d->reference_type = reference_type == ReferenceType::Callers
                          ? IDatabase::ReferenceType::Callers
                          : IDatabase::ReferenceType::Taint;

  static const bool kShouldIncludeRedeclarations{true};
  static const bool kGenerateRootNode{true};

  StartRequest(QModelIndex(), entity_id, d->reference_type,
               kShouldIncludeRedeclarations, kGenerateRootNode, kQueryDepth);

  emit beginResetModel();
  ResetContext(d->context);
  emit endResetModel();
}

void ReferenceExplorerModel::ExpandEntity(const QModelIndex &index) {
  if (!index.isValid()) {
    return;
  }

  CancelRunningRequest();

  auto entity_id_var = index.data(EntityIdRole);
  if (!entity_id_var.isValid()) {
    return;
  }

  auto entity_id = qvariant_cast<RawEntityId>(entity_id_var);

  static const bool kShouldIncludeRedeclarations{false};
  static const bool kGenerateRootNode{false};

  StartRequest(index, entity_id, d->reference_type,
               kShouldIncludeRedeclarations, kGenerateRootNode, kQueryDepth);
}

ReferenceExplorerModel::~ReferenceExplorerModel() {
  CancelRunningRequest();
}

QModelIndex ReferenceExplorerModel::index(int row, int column,
                                          const QModelIndex &parent) const {
  if (!hasIndex(row, column, parent)) {
    return QModelIndex();
  }

  std::uint64_t parent_node_id{};
  if (parent.isValid()) {
    parent_node_id = static_cast<std::uint64_t>(parent.internalId());
  }

  auto parent_node_it = d->context.tree.find(parent_node_id);
  if (parent_node_it == d->context.tree.end()) {
    return QModelIndex();
  }

  const auto &parent_node = parent_node_it->second;

  auto unsigned_row = static_cast<std::size_t>(row);
  if (unsigned_row >= parent_node.child_node_id_list.size()) {
    return QModelIndex();
  }

  auto child_node_id = parent_node.child_node_id_list[unsigned_row];
  if (d->context.tree.count(child_node_id) == 0) {
    return QModelIndex();
  }

  return createIndex(row, column, static_cast<quintptr>(child_node_id));
}

QModelIndex ReferenceExplorerModel::parent(const QModelIndex &child) const {
  if (!child.isValid()) {
    return QModelIndex();
  }

  auto child_node_id = static_cast<std::uint64_t>(child.internalId());

  auto child_node_it = d->context.tree.find(child_node_id);
  if (child_node_it == d->context.tree.end()) {
    return QModelIndex();
  }

  const auto &child_node = child_node_it->second;
  if (child_node.parent_node_id == 0) {
    return QModelIndex();
  }

  auto parent_node_it = d->context.tree.find(child_node.parent_node_id);
  if (parent_node_it == d->context.tree.end()) {
    return QModelIndex();
  }

  const auto &parent_node = parent_node_it->second;

  auto grandparent_node_id = parent_node.parent_node_id;

  auto grandparent_node_it = d->context.tree.find(grandparent_node_id);
  if (grandparent_node_it == d->context.tree.end()) {
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

  std::uint64_t parent_node_id{};
  if (parent.isValid()) {
    parent_node_id = static_cast<std::uint64_t>(parent.internalId());
  }

  auto parent_node_it = d->context.tree.find(parent_node_id);
  if (parent_node_it == d->context.tree.end()) {
    return 0;
  }

  const auto &parent_node = parent_node_it->second;
  return static_cast<int>(parent_node.child_node_id_list.size());
}

int ReferenceExplorerModel::columnCount(const QModelIndex &) const {
  return 3;
}

QVariant ReferenceExplorerModel::data(const QModelIndex &index,
                                      int role) const {
  if (!index.isValid()) {
    return QVariant();
  }

  auto node_id = static_cast<std::uint64_t>(index.internalId());
  auto node_it = d->context.tree.find(node_id);
  if (node_it == d->context.tree.end()) {
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
      if (node.data.opt_name.has_value()) {
        value = node.data.opt_name.value();

      } else {
        value = tr("Unnamed: ") + QString::number(node.data.entity_id);
      }

    } else if (column_number == 1) {
      if (node.data.opt_location.has_value()) {
        const auto &location = node.data.opt_location.value();

        std::filesystem::path file_path{location.path.toStdString()};
        auto file_name = QString::fromStdString(file_path.filename());

        value.setValue(QString("%1:%2:%3")
                           .arg(file_name, QString::number(location.line),
                                QString::number(location.column)));
      }

    } else if (column_number == 2) {
      if (node.data.opt_breadcrumbs.has_value()) {
        const auto &breadcrumbs = node.data.opt_breadcrumbs.value();
        value.setValue(breadcrumbs);
      }
    }

  } else if (role == Qt::ToolTipRole) {
    auto opt_decl_category = GetTokenCategory(d->index, node.data.entity_id);
    auto buffer =
        tr("Category: ") + GetTokenCategoryName(opt_decl_category) + "\n";

    buffer += tr("Entity ID: ") + QString::number(node.data.entity_id) + "\n";

    if (std::optional<FragmentId> frag_id =
            FragmentId::from(node.data.referenced_entity_id)) {

      buffer += tr("Fragment ID: ") +
                QString::number(EntityId(frag_id.value()).Pack());
    }

    if (node.data.opt_location.has_value()) {
      buffer += "\n";
      buffer +=
          tr("File ID: ") + QString::number(node.data.opt_location->file_id);
    }

    if (node.data.opt_location.has_value()) {
      const auto &location = node.data.opt_location.value();

      buffer += "\n";
      buffer += tr("Path: ") + location.path;
    }

    value = std::move(buffer);

  } else if (role == IReferenceExplorerModel::EntityIdRole) {
    value = static_cast<quint64>(node.data.entity_id);

  } else if (role == IReferenceExplorerModel::ReferencedEntityIdRole) {
    value = static_cast<quint64>(node.data.referenced_entity_id);

  } else if (role == IReferenceExplorerModel::FragmentIdRole) {
    if (std::optional<FragmentId> frag_id =
            FragmentId::from(node.data.referenced_entity_id)) {
      value.setValue(EntityId(frag_id.value()).Pack());
    }

  } else if (role == IReferenceExplorerModel::FileIdRole) {
    if (node.data.opt_location.has_value()) {
      value.setValue(node.data.opt_location->file_id);
    }

  } else if (role == IReferenceExplorerModel::LineNumberRole) {
    if (node.data.opt_location.has_value() &&
        0u < node.data.opt_location->line) {
      value.setValue(node.data.opt_location->line);
    }

  } else if (role == IReferenceExplorerModel::ColumnNumberRole) {
    if (node.data.opt_location.has_value() &&
        0u < node.data.opt_location->column) {
      value.setValue(node.data.opt_location->column);
    }

  } else if (role == IReferenceExplorerModel::TokenCategoryRole) {
    value.setValue(TokenCategory::FUNCTION);

  } else if (role == IReferenceExplorerModel::LocationRole) {
    if (node.data.opt_location.has_value()) {
      value.setValue(node.data.opt_location.value());
    }

  } else if (role == ReferenceExplorerModel::InternalIdentifierRole) {
    value.setValue(node_id);

  } else if (role == ReferenceExplorerModel::IconLabelRole) {
    auto opt_decl_category = GetTokenCategory(d->index, node.data.entity_id);

    auto label = GetTokenCategoryIconLabel(opt_decl_category);
    value.setValue(label);
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

ReferenceExplorerModel::ReferenceExplorerModel(
    const Index &index, const FileLocationCache &file_location_cache,
    QObject *parent)
    : IReferenceExplorerModel(parent),
      d(new PrivateData(index, file_location_cache)) {

  d->database = IDatabase::Create(index, file_location_cache);

  for (auto [path, id] : d->index.file_paths()) {
    d->file_path_map.emplace(id.Pack(),
                             QString::fromStdString(path.generic_string()));
  }

  connect(&d->future_watcher, &QFutureWatcher<QFuture<bool>>::finished, this,
          &ReferenceExplorerModel::FutureResultStateChanged);

  connect(&d->import_timer, &QTimer::timeout, this,
          &ReferenceExplorerModel::ProcessDataBatchQueue);
}

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

void ReferenceExplorerModel::ImportReferences(
    IDatabase::QueryEntityReferencesResult result) {

  bool reset_model{false};

  if (d->context.tree.empty()) {
    d->context.tree.insert({0, Context::Node{}});
    reset_model = true;
  }

  auto insert_point_node_id = d->insert_point.internalId();
  auto orig_insert_point_node = d->context.tree[insert_point_node_id];
  if (!orig_insert_point_node.child_node_id_list.empty()) {
    reset_model = true;
  }

  for (auto &database_node : result.node_list) {
    Assert(
        d->node_id_mapping.count(database_node.mapping_info.parent_node_id) > 0,
        "Unknown node id");

    auto parent_node_id =
        d->node_id_mapping[database_node.mapping_info.parent_node_id];

    auto &parent_node = d->context.tree[parent_node_id];

    if (parent_node.entity_id_node_id_map.count(database_node.entity_id) > 0) {
      auto node_id = parent_node.entity_id_node_id_map[database_node.entity_id];
      d->node_id_mapping.insert({database_node.mapping_info.node_id, node_id});
      continue;
    }

    auto node_id = GenerateNodeID(d->context);
    parent_node.entity_id_node_id_map.insert(
        {database_node.entity_id, node_id});

    parent_node.child_node_id_list.push_back(node_id);

    Context::Node node;
    node.data = std::move(database_node);
    node.node_id = node_id;
    node.parent_node_id = parent_node_id;

    d->node_id_mapping.insert(
        {database_node.mapping_info.node_id, node.node_id});

    d->context.tree.insert({node_id, std::move(node)});
  }

  if (reset_model) {
    emit beginResetModel();
    emit endResetModel();

  } else {
    const auto &current_insert_point_node =
        d->context.tree[insert_point_node_id];

    auto first_row =
        static_cast<int>(current_insert_point_node.child_node_id_list.size() -
                         orig_insert_point_node.child_node_id_list.size());
    auto last_row =
        static_cast<int>(orig_insert_point_node.child_node_id_list.size() - 1);

    emit beginInsertRows(d->insert_point, first_row, last_row);
  }
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

void ReferenceExplorerModel::StartRequest(
    const QModelIndex &insert_point, const RawEntityId &entity_id,
    const IDatabase::ReferenceType &reference_type,
    const bool &include_redeclarations, const bool &emit_root_node,
    const std::size_t &depth) {

  d->insert_point = insert_point;

  d->node_id_mapping.clear();
  d->node_id_mapping.insert({0, d->insert_point.internalId()});

  d->request_status_future = d->database->QueryEntityReferences(
      *this, entity_id, reference_type, include_redeclarations, emit_root_node,
      depth);

  d->future_watcher.setFuture(d->request_status_future);
  d->import_timer.start(kFirstUpdateInterval);

  emit RequestStarted();
}

void ReferenceExplorerModel::OnDataBatch(DataBatch data_batch) {
  std::lock_guard<std::mutex> lock(d->data_batch_queue_mutex);

  d->data_batch_queue.insert(d->data_batch_queue.end(),
                             std::make_move_iterator(data_batch.begin()),
                             std::make_move_iterator(data_batch.end()));
}

void ReferenceExplorerModel::CancelRunningRequest() {
  auto is_importing = d->import_timer.isActive();
  auto is_querying = d->request_status_future.isRunning();

  d->import_timer.stop();
  d->data_batch_queue.clear();

  if (d->request_status_future.isRunning()) {
    d->request_status_future.cancel();
    d->request_status_future.waitForFinished();
    d->request_status_future = {};
  }

  if (is_importing || is_querying) {
    emit RequestFinished();
  }
}

void ReferenceExplorerModel::ProcessDataBatchQueue() {
  DataBatch data_batch_queue;

  {
    std::lock_guard<std::mutex> lock(d->data_batch_queue_mutex);

    data_batch_queue = std::move(d->data_batch_queue);
    d->data_batch_queue.clear();
  }

  // Merge all the updates together so we don't have to emit
  // many expensive beginInsertRows/endInsertRows. The slowness
  // is made worse by the proxy models that we are using
  // (QSortFilterProxyModel + IGlobalHighlighter), so make
  // sure that the view disables dynamic sorting while the
  // request is running!
  IDatabase::QueryEntityReferencesResult result;
  for (auto &queue_entry : data_batch_queue) {
    result.node_list.insert(
        result.node_list.end(),
        std::make_move_iterator(queue_entry.node_list.begin()),
        std::make_move_iterator(queue_entry.node_list.end()));
  }

  ImportReferences(std::move(result));

  if (!d->request_status_future.isRunning() && d->data_batch_queue.empty()) {
    d->import_timer.stop();
    emit RequestFinished();

  } else {
    // Restart the timer, so that the import procedure will fire again
    // in kImportInterval msecs from the end of the previous batch
    d->import_timer.start(kImportInterval);
  }
}

void ReferenceExplorerModel::FutureResultStateChanged() {
  if (d->request_status_future.isCanceled()) {
    emit beginResetModel();
    emit endResetModel();

    return;
  }

  auto request_status = d->request_status_future.takeResult();
  if (!request_status) {
    emit beginResetModel();
    emit endResetModel();

    return;
  }
}


}  // namespace mx::gui
