/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/ICodeModel.h>

#include <multiplier/File.h>
#include <multiplier/Index.h>

#include <memory>
#include <variant>
#include <unordered_map>

namespace mx::gui {

//! Main implementation for the ICodeModel interface
class CodeModel final : public ICodeModel {
  Q_OBJECT

 public:
  //! Destructor
  virtual ~CodeModel() override;

  //! \copybrief ICodeModel::GetEntity
  virtual std::optional<RawEntityId> GetEntity(void) const override;

  //! \copybrief ICodeModel::SetEntity
  virtual void SetEntity(RawEntityId id) override;

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
        std::uint64_t line_number{};
        ID child_id{};
      };

      //! Contains the tokens displayed by the code view
      struct ColumnListData final {
        struct Column final {
          RawEntityId token_id;
          RawEntityId related_entity_id;
          RawEntityId statement_entity_id;
          TokenCategory token_category;
          QString data;
          std::optional<std::uint64_t> opt_token_group_id;
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
  };

  //! Generates a new node id
  static Context::Node::ID GenerateNodeID(Context &context);

 protected:
  //! Constructor
  CodeModel(const FileLocationCache &file_location_cache, const Index &index,
            QObject *parent);

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Aborts the active request
  void CancelRunningRequest();

 private slots:
  //! Called when the async database request is ready
  void FutureResultStateChanged();

  friend class ICodeModel;
};

}  // namespace mx::gui
