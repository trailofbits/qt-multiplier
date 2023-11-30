/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include "ITreeExplorerExpansionThread.h"

#include <multiplier/ui/IGeneratorModel.h>

#include <multiplier/Types.h>

#include <QList>

#include <unordered_map>

namespace mx::gui {

//! Implements the ITreeExplorerModel interface
class GeneratorModel final : public IGeneratorModel {
  Q_OBJECT

  //! Private data
  struct PrivateData;

  //! Instance data
  std::unique_ptr<PrivateData> d;

 public:
  //! Constructor
  GeneratorModel(QObject *parent);

  //! This constructor creates a new model, using `source_model` as a base
  //! and `root_item` as the new root item
  GeneratorModel(const GeneratorModel &source_model,
                 const QModelIndex &root_item, QObject *parent);

  //! Common code for all constructors
  void InitializeModel();

  //! Destructor
  virtual ~GeneratorModel(void);

  //! Install a new generator to back the data of this model.
  void
  InstallGenerator(std::shared_ptr<ITreeGenerator> generator_) Q_DECL_FINAL;

  //! Expands the given entity
  virtual void Expand(const QModelIndex &index,
                      const std::size_t &depth) override;

  //! Find the original version of an item.
  virtual QModelIndex Deduplicate(const QModelIndex &index) override;

  //! Creates a new Qt model index
  QModelIndex index(int row, int column,
                    const QModelIndex &parent) const Q_DECL_FINAL;

  //! Returns the parent of the given model index
  QModelIndex parent(const QModelIndex &child) const Q_DECL_FINAL;

  //! Returns the amount or rows in the model
  //! Since this is a tree model, rows are intended as child items
  int rowCount(const QModelIndex &parent) const Q_DECL_FINAL;

  //! Returns the amount of columns in the model
  int columnCount(const QModelIndex &parent) const Q_DECL_FINAL;

  //! Returns the index data for the specified role
  //! \todo Fix role=TokenCategoryRole support
  QVariant data(const QModelIndex &index, int role) const Q_DECL_FINAL;

  //! Returns the column names for the tree view header
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role) const Q_DECL_FINAL;

 private:
  //! Starts the given expansion thread
  void RunExpansionThread(ITreeExplorerExpansionThread *thread);

 private slots:
  //! Notifies us when there's a batch of new data to update.
  void OnNewTreeItems(uint64_t version_number, RawEntityId parent_entity_id,
                      QList<std::shared_ptr<ITreeItem>> child_items,
                      unsigned remaining_depth);

  //! Processes the entire data batch queue
  void ProcessDataBatchQueue(void);

  //! Called when the tree title has been resolved.
  void OnNameResolved(void);

 public slots:
  //! Cancels any active request
  void CancelRunningRequest() Q_DECL_FINAL;

 public:
  //! The stateful data for this model
  struct Context final {
    //! A single node and all its attributes
    struct Node final {
      //! Unique node ID
      using ID = std::uint64_t;

      //! Node state
      enum class State {
        Unopened,
        Opened,
        Opening,
      };

      //! Node data
      struct Data final {
        //! A list of column values
        using ColumnValueList = std::vector<QVariant>;

        //! The column values returned by the generator; the column name can
        //! be acquired directly from the generator
        ColumnValueList column_value_list;

        //! Multiplier entity ID
        RawEntityId entity_id;
      };

      //! The ID of the internal root item
      static const ID kRootNodeID;

      //! The ID for this node
      ID node_id{};

      //! Parent node id
      ID parent_node_id{};

      //! A list of the child nodes
      std::vector<ID> child_node_id_list;

      //! The baked row number of this node
      int row{};

      //! This node could optionally be an alias for another entity
      std::optional<RawEntityId> opt_aliased_entity_id;

      //! Current node state
      State state{State::Unopened};

      //! Node data
      Data data;
    };

    //! A subtree that needs to be inserted into the model tree
    struct Subtree final {
      //! A shared pointer to an ITreeItem item
      //! \todo Remove shared_ptr usage
      using ITreeItemPtr = std::shared_ptr<ITreeItem>;

      //! The parent node id
      Node::ID parent_node_id{};

      //! A list of ITreeItemPtr objects
      std::vector<ITreeItemPtr> tree_item_list;

      //! This is used to queue up additional recursive expansions
      std::size_t remaining_depth{};
    };

    //! A list of subtrees that need to be inserted into the tree
    using SubtreeList = std::vector<Subtree>;

    //! The node ID generator
    Node::ID node_id_generator{1};

    //! Maps a raw entity id to a node id
    std::unordered_map<RawEntityId, Node::ID> mx_entity_id_to_node_id;

    //! The node tree that contains the model data
    std::unordered_map<Node::ID, Node> tree;

    //! Column title list
    std::vector<QString> column_title_list;

    //! A list of subtrees that need to be inserted
    SubtreeList incoming_subtree_list;

    //! The name of this tree
    QString tree_name;
  };

  //! Resets the context structure, initializing a root item
  static void InitializeContext(Context &context,
                                const std::vector<QString> &column_title_list);

  //! Generates a new node id
  static Context::Node::ID GenerateNodeID(Context &context);

  //! Returns a pointer to the given node, if any
  static Context::Node *GetNodeFromID(Context &context,
                                      const Context::Node::ID &node_id);

  //! Returns the node id for the given mx entity id
  static std::optional<Context::Node::ID>
  GetNodeIDFromMxEntityID(const Context &context, const RawEntityId &entity_id);

  //! Returns the model index for the specified node id
  //! \note This can't be `static` since we need to call ::createIndex
  QModelIndex GetModelIndexFromNodeID(Context &context,
                                      const Context::Node::ID &node_id,
                                      const int &column) const;

  //! Imports the subtrees scheduled for merging inside the Context struct
  //! \note This can't be `static` since we need to call `GetModelIndexFromNodeID`
  void ImportIncomingSubtreeList(
      Context &context,
      const std::optional<std::size_t> &opt_max_subtree_count = std::nullopt);

  //! Dereferences the given node id
  static std::optional<Context::Node::ID>
  DereferenceNodeID(const Context &context, Context::Node::ID node_id);

  //! Returns the node id for the given entity id
  static std::optional<Context::Node::ID>
  GetNodeIDFromEntityID(const Context &context, const RawEntityId &entity_id);

  //! Imports a subtree from the given context
  static bool ImportContextSubtree(Context &dest_context,
                                   const Context &source_context,
                                   const Context::Node::ID &root_node_id);
};

}  // namespace mx::gui
