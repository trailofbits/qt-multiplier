/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <filesystem>
#include <multiplier/Entity.h>
#include <QDataStream>
#include <QAbstractItemModel>
#include <QMetaType>
#include <QString>
#include <unordered_map>

namespace mx {
class FileLocationCache;
class Index;
namespace gui {

//! A model for the reference explorer widget
class IReferenceExplorerModel : public QAbstractItemModel {
  Q_OBJECT

 public:

  //! Expansion modes.
  enum ExpansionMode {
    //! A node whose current expansion mode has already been activated. This
    //! is used to prevent us from repeatedly expanding the same node.
    AlreadyExpanded,

    //! Expand showing the call hierarchy.
    CallHierarchyMode
  };

  //! Additional item data roles for this model
  enum ItemDataRole {
    //! Returns a Location object
    LocationRole = Qt::UserRole + 1,

    //! Returns the default expansion mode for this node's children.
    DefaultExpansionMode,

    //! Tells us whether or not this node has been expanded.
    HasBeenExpanded,

    //! Returns the internal node identifier
    InternalIdentifierRole,

    //! Returns the entity id as a RawEntityId value
    EntityIdRole,

    //! Returns the referenced entity id as a RawEntityId value
    ReferencedEntityIdRole,

    //! Returns the fragment id as a RawEntityId value
    FragmentIdRole,

    //! Returns the file id as a RawEntityId value
    FileIdRole,

    //! Returns the line number id as an unsigned value
    LineNumberRole,

    //! Returns the column number id as an unsigned value
    ColumnNumberRole,
  };

  //! Location information, containing path + line and column
  struct Location final {
    RawEntityId file_id{kInvalidEntityId};

    //! File path
    QString path;

    //! An optional line number
    unsigned line{0};

    //! An optional column number
    unsigned column{0};

    friend QDataStream &operator<<(QDataStream &stream, const Location &self);

    // NOTE(pag): May throw.
    friend QDataStream &operator>>(QDataStream &stream, Location &self);
  };


  //! A single node in the model
  struct Node final {
    static QString kMimeTypeName;

    //! How this node was imported
    IReferenceExplorerModel::ExpansionMode expansion_mode{
        IReferenceExplorerModel::CallHierarchyMode};

    // Create and initialize a node.
    //
    // NOTE(pag): This is a blocking operation.
    static Node Create(const FileLocationCache &file_cache,
                       const VariantEntity &entity,
                       const VariantEntity &referenced_entity,
                       IReferenceExplorerModel::ExpansionMode import_mode);

    // Initialize this node with a specific parent id node.
    void AssignUniqueId(void);

    //! The id for this node
    std::uint64_t node_id{};

    //! The parent node id
    std::uint64_t parent_node_id{};

    //! Multiplier's entity id
    RawEntityId entity_id{kInvalidEntityId};

    //! Multiplier's referenced entity id
    RawEntityId referenced_entity_id{kInvalidEntityId};

    //! An optional name for this entity
    std::optional<QString> opt_name;

    //! Optional file location information (path + line + column)
    std::optional<IReferenceExplorerModel::Location> opt_location;

    //! Child nodes
    std::vector<std::uint64_t> child_node_id_list;

    friend QDataStream &operator<<(QDataStream &stream, const Node &self);

    // NOTE(pag): May throw.
    friend QDataStream &operator>>(QDataStream &stream, Node &self);
  };

  //! A node tree representing the model data
  struct NodeTree final {

    //! A map containing all the nodes in the tree, indexed by their unique
    //! node IDs.
    std::unordered_map<std::uint64_t, Node> node_map;

    //! The id of the root node. There are two separate IDs because we allow the
    //! tree to be "re-rooted." `root_node_id` reflects the true root of the tree,
    //! and `curr_root_node_id` reflects the current active / visible root.
    const std::uint64_t root_node_id;
    std::uint64_t curr_root_node_id;

    // Reset the tree and prevent the practical re-use of node IDs.
    NodeTree(void);

    Node *CurrentRootNode(void);
    const Node *CurrentRootNode(void) const;
  };

  //! Factory method
  static IReferenceExplorerModel *
  Create(mx::Index index, mx::FileLocationCache file_location_cache,
         QObject *parent = nullptr);

  //! Expands the specified entity
  virtual void ExpandEntity(const QModelIndex &index) = 0;

  //! Removes the specified entity and all of its children.
  virtual void RemoveEntity(const QModelIndex &index) = 0;

  //! Returns true if an alternative root is being used
  virtual bool HasAlternativeRoot() const = 0;

  //! Sets the given item as the new root
  virtual void SetRoot(const QModelIndex &index) = 0;

  //! Restores the default root item
  virtual void SetDefaultRoot() = 0;

  //! Constructor
  IReferenceExplorerModel(QObject *parent) : QAbstractItemModel(parent) {}

  //! Destructor
  virtual ~IReferenceExplorerModel() override = default;

 public slots:

  //! Adds a new entity object under the given parent
  virtual void AppendEntityById(
      RawEntityId entity_id, ExpansionMode import_mode,
      const QModelIndex &parent) = 0;

 private:

  //! Disabled copy constructor
  IReferenceExplorerModel(const IReferenceExplorerModel &) = delete;

  //! Disabled copy assignment operator
  IReferenceExplorerModel &operator=(const IReferenceExplorerModel &) = delete;
};

}  // namespace gui
}  // namespace mx

//! Allows mx::gui::IReferenceExplorerModel::Location values to fit inside QVariant objects
Q_DECLARE_METATYPE(mx::gui::IReferenceExplorerModel::Location);
Q_DECLARE_METATYPE(mx::gui::IReferenceExplorerModel::Node);
Q_DECLARE_METATYPE(mx::gui::IReferenceExplorerModel::ExpansionMode);
