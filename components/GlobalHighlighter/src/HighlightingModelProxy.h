/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include "Types.h"

#include <QAbstractItemModel>

#include <unordered_map>

namespace mx::gui {

//! A model proxy used to signal views which tokens to highlight
class HighlightingModelProxy final : public QAbstractItemModel {
  Q_OBJECT

 public:
  HighlightingModelProxy(QAbstractItemModel *source_model,
                         int entity_id_data_role);

  virtual ~HighlightingModelProxy() override;

  //! Creates a new Qt model index
  virtual QModelIndex index(int row, int column,
                            const QModelIndex &parent) const override;

  //! Returns the parent of the given model index
  virtual QModelIndex parent(const QModelIndex &child) const override;

  //! Returns the amount or rows in the model
  virtual int rowCount(const QModelIndex &parent) const override;

  //! Returns the amount of columns in the model
  virtual int columnCount(const QModelIndex &parent) const override;

  //! Returns the index data for the specified role
  virtual QVariant data(const QModelIndex &index, int role) const override;

  //! Node tree
  struct Context final {
    //! A single node in the tree
    struct Node final {
      //! Node ID type
      using ID = std::uint64_t;

      //! The ID for this node
      ID id{};

      //! The ID for the parent node
      ID parent_id{};

      //! \brief The `row,0` index of the source model
      //! This always point to the first column. Make sure
      //! to regenerate the index from the parent when using
      //! a different column
      QModelIndex source_index;

      //! A list of child node IDs
      std::vector<ID> child_id_list;
    };

    //! Node ID generator
    Node::ID id_generator{};

    //! The node tree stored as a map
    std::unordered_map<Node::ID, Node> node_map;

    //! A list of all the entity IDs that should be highlighted
    EntityHighlightList entity_highlight_list;
  };

  //! Generates a new node ID
  static Context::Node::ID GenerateNodeID(Context &context);

  //! Creates a new index for the source model
  static void GenerateModelIndex(Context &context,
                                 const QAbstractItemModel &source_model);

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public slots:
  //! Triggers a model reset without a source model reindex
  void OnEntityHighlightListChange(const EntityHighlightList &);

 private slots:
  //! Triggers a source model reindex + a model reset signal
  void OnColumnsInserted(const QModelIndex &parent, int first, int last);

  //! Triggers a source model reindex + a model reset signal
  void OnColumnsMoved(const QModelIndex &parent, int start, int end,
                      const QModelIndex &destination, int column);

  //! Triggers a source model reindex + a model reset signal
  void OnColumnsRemoved(const QModelIndex &parent, int first, int last);

  //! Triggers a source model reindex + a model reset signal
  void OnDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight,
                     const QList<int> &roles);

  //! Triggers a source model reindex + a model reset signal
  void OnHeaderDataChanged(Qt::Orientation orientation, int first, int last);

  //! Triggers a source model reindex + a model reset signal
  void OnLayoutChanged(const QList<QPersistentModelIndex> &parents,
                       QAbstractItemModel::LayoutChangeHint hint);

  //! Triggers a source model reindex + a model reset signal
  void OnModelReset();

  //! Triggers a source model reindex + a model reset signal
  void OnRowsInserted(const QModelIndex &parent, int first, int last);

  //! Triggers a source model reindex + a model reset signal
  void OnRowsMoved(const QModelIndex &parent, int start, int end,
                   const QModelIndex &destination, int row);

  //! Triggers a source model reindex + a model reset signal
  void OnRowsRemoved(const QModelIndex &parent, int first, int last);

  //! Triggers a source model reindex + a model reset signal
  void GenerateModelIndex();

 signals:
  //! This model only supports full resets
  void modelReset();
};

}  // namespace mx::gui
