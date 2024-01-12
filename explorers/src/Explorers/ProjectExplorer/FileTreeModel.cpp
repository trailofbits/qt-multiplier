/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "FileTreeModel.h"

#include <QApplication>
#include <QFont>
#include <QPalette>

#include <deque>
#include <iostream>
#include <map>
#include <set>
#include <vector>

#include <multiplier/Index.h>
#include <multiplier/Frontend/TokenCategory.h>
#include <multiplier/Frontend/TokenKind.h>

namespace mx::gui {
namespace {

//! A single node in the internal tree
struct Node final {
  QString name;
  QString full_path;
  RawEntityId file_id{kInvalidEntityId};
  Node *parent{nullptr};
  QList<Node *> children;
  int row{0};
  unsigned name_token_index{0};
};

}  // namespace

struct FileTreeModel::PrivateData {
  Index index;
  std::deque<Node> nodes;
  Node root_node;
  Node custom_root_node;
  Node *current_root_node;
  TokenRange name_tokens;

  int saved_row{0};
  Node *saved_parent{nullptr};

  inline PrivateData(void)
      : current_root_node(&root_node) {
    custom_root_node.children.push_back(nullptr);      
  }
};

FileTreeModel::~FileTreeModel(void) {}

FileTreeModel::FileTreeModel(QObject *parent)
    : IModel(parent),
      d(new PrivateData) {}

void FileTreeModel::SetIndex(const Index &index) {
  d->index = index;
  SetRoot(QModelIndex());

  std::deque<Node> new_nodes;
  std::vector<CustomToken> name_tokens;
  FilePathMap files = index.file_paths();

  emit beginResetModel();

  std::set<std::filesystem::path> has_files;
  for (const auto &[path_, file_id] : files) {
    const std::filesystem::path &path = path_;
    auto base = path.root_path();
    for (std::filesystem::path part : path.parent_path()) {
      base /= part;  // A bit redundant, but follow the same process throughout.
    }
    has_files.emplace(std::move(base));
  }

  // Group the paths into folders that have at least one file in them. This is
  // so that we don't have to see crazy deep folder trees all the time.
  std::map<std::filesystem::path, FilePathMap> sub_lists;
  for (const auto &[path_, file_id] : files) {
    std::filesystem::path path = path_;
    auto base = path.root_path();
    for (std::filesystem::path part : path.parent_path()) {
      base /= part;
      if (has_files.count(base)) {
        sub_lists[base].emplace(std::move(path), file_id);
        break;
      }
    }
  }

  // Build up the items.
  for (const auto &[parent_path_, sub_list] : sub_lists) {
    const std::filesystem::path &parent_path = parent_path_;

    Node *root_item = &(new_nodes.emplace_back());
    root_item->parent = &(d->root_node);
    root_item->full_path = QString::fromStdString(parent_path.generic_string());
    root_item->name = root_item->full_path;
    root_item->row = static_cast<int>(d->root_node.children.size());
    d->root_node.children.push_back(root_item);

    std::map<std::filesystem::path, Node *> item_map;

    for (const auto &[path_, file_id] : sub_list) {
      std::filesystem::path path = path_;
      std::filesystem::path base;
      std::filesystem::path full_base = path;
      Node *last = root_item;
      for (std::filesystem::path part : path.lexically_relative(parent_path)) {
        base /= part;
        full_base /= part;
        auto &item = item_map[base];
        if (!item) {
          item = &(new_nodes.emplace_back());
          item->parent = last;
          item->full_path = QString::fromStdString(full_base.generic_string());
          item->name = QString::fromStdString(part.generic_string());
          item->row = static_cast<int>(last->children.size());
          last->children.push_back(item);
        }
        last = item;
      }

      item_map.erase(base);
      last->file_id = file_id.Pack();
      last->full_path = QString::fromStdString(path.generic_string());
      last->name_token_index = static_cast<unsigned>(name_tokens.size());

      UserToken tok;
      tok.kind = TokenKind::HEADER_NAME;
      tok.category = TokenCategory::FILE_NAME;
      tok.data = last->name.toStdString();
      name_tokens.emplace_back(std::move(tok));
    }
  }

  // Publish the changes.
  d->nodes.swap(new_nodes);
  d->current_root_node = &(d->root_node);
  d->name_tokens = TokenRange::create(std::move(name_tokens));

  emit endResetModel();
}

bool FileTreeModel::HasAlternativeRoot(void) const {
  return d->current_root_node != &(d->root_node);
}

void FileTreeModel::SetRoot(const QModelIndex &index) {
  emit beginResetModel();

  if (d->current_root_node == &(d->custom_root_node)) {
    auto prev_custom = d->custom_root_node.children[0];
    prev_custom->row = d->saved_row;
    prev_custom->parent = d->saved_parent;
  }

  if (!index.isValid()) {
    d->current_root_node = &(d->root_node);

  } else {
    auto node = reinterpret_cast<Node *>(index.internalPointer());
    d->saved_row = node->row;
    d->saved_parent = node->parent;

    node->row = 0;
    node->parent = &(d->custom_root_node);

    d->current_root_node = &(d->custom_root_node);
    d->custom_root_node.children[0] = node;
  }

  emit endResetModel();
}

void FileTreeModel::SetDefaultRoot(void) {
  SetRoot(QModelIndex());
}

QModelIndex FileTreeModel::index(int row, int column,
                                 const QModelIndex &parent) const {

  if (!hasIndex(row, column, parent) || column || 0 > row) {
    return {};
  }

  Node *parent_node = d->current_root_node;
  if (parent.isValid()) {
    parent_node = reinterpret_cast<Node *>(parent.internalPointer());
  }

  if (parent_node->file_id != kInvalidEntityId ||
      row >= parent_node->children.size()) {
    return {};
  }

  return createIndex(row, column, parent_node->children[row]);
}

QModelIndex FileTreeModel::parent(const QModelIndex &child) const {
  if (!child.isValid()) {
    return {};
  }

  // Get the child node
  auto child_node = reinterpret_cast<Node *>(child.internalPointer());
  auto parent_node = child_node->parent;
  if (parent_node == d->current_root_node) {
    return {};
  }

  return createIndex(parent_node->row, 0, parent_node);
}

int FileTreeModel::rowCount(const QModelIndex &parent) const {
  if (parent.column() >= 1) {
    return 0;
  }

  Node *parent_node = d->current_root_node;
  if (parent.isValid()) {
    parent_node = reinterpret_cast<Node *>(parent.internalPointer());
  }

  return static_cast<int>(parent_node->children.size());
}

int FileTreeModel::columnCount(const QModelIndex &) const {
  return d->root_node.children.empty() ? 0 : 1;
}

QVariant FileTreeModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid()) {
    return {};
  }

  auto node = reinterpret_cast<Node *>(index.internalPointer());

  if (role == AbsolutePathRole || role == Qt::ToolTipRole) {
    return node->full_path;
  
  } else if (role == Qt::DisplayRole) {
    return node->name;

  } else if (role == IModel::EntityRole) {
    if (node->file_id != kInvalidEntityId) {
      if (auto file = d->index.file(node->file_id)) {
        return QVariant::fromValue<VariantEntity>(file.value());
      }
    }

  } else if (role == IModel::ModelIdRole) {
    return ConstantModelId();
  
  } else if (role == IModel::TokenRangeDisplayRole) {
    if (node->file_id != kInvalidEntityId) {
      return QVariant::fromValue(
          d->name_tokens.slice(node->name_token_index,
                               node->name_token_index + 1u));
    }

  } else if (role == FileIdRole) {
    if (node->file_id != kInvalidEntityId) {
      return node->file_id;
    }
  }

  return {};
}

}  // namespace mx::gui
