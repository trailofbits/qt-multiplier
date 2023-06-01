/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/ICodeModel.h>

#include <multiplier/Entities/TokenCategory.h>

#include <QAbstractItemModel>

#include <memory>
#include <unordered_map>

namespace mx::gui {

//! A code view model that displays data for the reference explorer
class RefExplorerToCodeViewModelAdapter final : public ICodeModel {
  Q_OBJECT

 public:
  //! Additional item data roles
  //! \note Make sure this is not overlapping the ICodeModel roles
  enum ItemDataRole { OriginalModelIndex = Qt::UserRole + 100, IsExpandButton };

  //! Constructor
  RefExplorerToCodeViewModelAdapter(QAbstractItemModel *model, QObject *parent);

  //! Destructor
  virtual ~RefExplorerToCodeViewModelAdapter() override;

  //! Enables or disables breadcrumbs
  void SetBreadcrumbsVisibility(const bool &enable);

  //! \copybrief ICodeModel::SetEntity
  //! \todo This method needs to be removed from the interface
  [[noreturn]] virtual void SetEntity(RawEntityId id) override;

  //! \copybrief ICodeModel::IsReady
  virtual bool IsReady() const override;

  //! Creates a new Qt model index
  virtual QModelIndex index(int row, int column,
                            const QModelIndex &parent) const override;

  //! Returns the parent of the given model index
  virtual QModelIndex parent(const QModelIndex &child) const override;

  //! Returns the amount of rows in the given parent item
  virtual int rowCount(const QModelIndex &parent) const override;

  //! Returns the amount of columns for the given parent item
  virtual int columnCount(const QModelIndex &parent) const override;

  //! Returns the data for the specified role
  virtual QVariant data(const QModelIndex &index, int role) const override;

  //! Contains all the nodes in the model
  struct Context final {
    //! A single node
    struct Node final {
      //! Node identifier
      using ID = std::uint64_t;

      //! A list of node IDs. Use to list child nodes
      using IDList = std::vector<ID>;

      //! The root node only has a child id list
      struct RootData final {
        IDList child_id_list;
      };

      //! A line node references the original index and the column list node
      struct LineData final {
        unsigned line_number{};
        QModelIndex original_model_index;
        ID child_id{};
      };

      //! Contains the tokens displayed by the code view
      struct ColumnListData final {
        struct Column final {
          TokenCategory token_category;
          QString data;
          bool is_expand_button{false};
        };

        using ColumnList = std::vector<Column>;

        ColumnList column_list;
      };

      //! Node data
      using NodeData =
          std::variant<std::monostate, RootData, LineData, ColumnListData>;

      //! The id of this node
      ID id{};

      //! Parent node id
      ID parent_id{};

      //! Node data
      NodeData data;
    };

    //! Node ID generator
    Node::ID node_id_generator{};

    //! The node map. Node 0 is always the root
    std::unordered_map<Node::ID, Node> node_map;

    //! True if the breadcrumbs should be generated
    bool breadcrumbs_enabled{false};
  };

  //! Imports the given model by generating tokens for the code view
  static void ImportReferenceExplorerModel(Context &context,
                                           const QAbstractItemModel *model);

  //! Generates a new node id
  static Context::Node::ID GenerateNodeID(Context &context);

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 private slots:
  //! Used to invalidate the code view tokens in response to model changes
  void OnModelChange();
};

}  // namespace mx::gui
